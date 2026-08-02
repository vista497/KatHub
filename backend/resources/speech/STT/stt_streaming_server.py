"""
STT Server с потоковым распознаванием и транскрипцией файлов
УЛУЧШЕННАЯ ВЕРСИЯ на основе анализа рабочего Python кода
- Упрощенная VAD без разбиения на части
- Встроенный VAD фильтр Whisper
- Полное сохранение начала фраз
- Распознавание по файлу (команда: transcribe_file)
"""

import socket
import os
import json
import threading
import base64
import numpy as np
from faster_whisper import WhisperModel
import torch
import queue
import time
import sys

class StreamingSession:
    """Сессия потокового распознавания для клиента"""
    
    def __init__(self, client_socket, whisper_model, vad_model, sample_rate=16000, vad_device="cuda"):
        self.client_socket = client_socket
        self.whisper = whisper_model
        self.vad_model = vad_model
        self.sample_rate = sample_rate
        self.vad_device = vad_device
        
        self.audio_buffer = []
        self.is_speaking = False
        self.silence_chunks = 0
        self.silence_threshold = 30  # ~1 секунда тишины
        
        self.running = False
        self.language = "ru"
        
        # Очередь для асинхронной обработки транскрипции
        self.transcription_queue = queue.Queue(maxsize=2)
        self.transcription_thread = threading.Thread(target=self._transcription_worker, daemon=True)
        self.transcription_thread.start()
        
    def detect_speech(self, audio_chunk):
        if self.vad_model is None:
            volume = np.abs(audio_chunk).mean()
            return volume > 0.01

        try:
            volume = np.abs(audio_chunk).mean()
            if volume < 0.005:
                return False

            if audio_chunk.dtype != np.float32:
                audio_chunk = audio_chunk.astype(np.float32)

            if np.abs(audio_chunk).max() > 1.0:
                audio_chunk = audio_chunk / 32768.0

            # ← Вот ключевое изменение: режем на чанки по 512
            chunk_size = 512 if self.sample_rate == 16000 else 256
            has_speech = False

            for i in range(0, len(audio_chunk), chunk_size):
                chunk = audio_chunk[i:i + chunk_size]

                if len(chunk) < chunk_size:
                    # Последний неполный чанк — пропускаем или паддим нулями (лучше паддить)
                    pad_len = chunk_size - len(chunk)
                    chunk = np.pad(chunk, (0, pad_len), mode='constant')
                
                audio_tensor = torch.from_numpy(chunk).to(self.vad_device, non_blocking=True)
                
                with torch.no_grad():
                    prob = self.vad_model(audio_tensor.unsqueeze(0), self.sample_rate).item()  # ← добавь .unsqueeze(0) для batch dim
                
                if prob > 0.3:
                    has_speech = True
                    break  # можно break, если достаточно любого speech-чанка

            return has_speech

        except Exception as e:
            print(f"VAD error: {e}")
            return np.abs(audio_chunk).mean() > 0.01
    
    def _transcription_worker(self):
        """Рабочий поток для асинхронной транскрипции"""
        while True:
            try:
                audio_data = self.transcription_queue.get(timeout=1.0)
                if audio_data is None:  # Сигнал завершения
                    break
                
                results = self._transcribe_chunk_internal(audio_data)
                
                if results:
                    self.send_result({
                        'event': 'transcription',
                        'results': results,
                        'timestamp': time.time()
                    })
                    
            except queue.Empty:
                continue
            except Exception as e:
                print(f"Transcription worker error: {e}")
    
    def _transcribe_chunk_internal(self, audio_data):
        """
        Внутренняя функция распознавания
        КЛЮЧ: Используем встроенный VAD фильтр Whisper!
        """
        try:
            if audio_data.dtype != np.float32:
                audio_data = audio_data.astype(np.float32)
            if np.abs(audio_data).max() > 1.0:
                audio_data = audio_data / 32768.0
            
            # Убедимся, что у нас достаточно данных
            if len(audio_data) < 800:  # Минимум 0.1 секунды
                return []
            
            # КЛЮЧЕВОЕ ОТЛИЧИЕ: Включаем встроенный VAD фильтр!
            # Он точно определяет границы речи и не теряет начало
            segments, info = self.whisper.transcribe(
                audio_data,
                language=self.language,
                beam_size=1,  # Быстро
                best_of=1,
                temperature=0.0,
                vad_filter=True,  # ✅ ВКЛЮЧЕН встроенный VAD!
                vad_parameters=dict(
                    threshold=0.3,  # Порог обнаружения речи
                    min_speech_duration_ms=250,  # Минимальная длительность речи
                    min_silence_duration_ms=100  # Минимальная пауза
                ),
                word_timestamps=False,
                condition_on_previous_text=False,
                initial_prompt=None,
                without_timestamps=True
            )
            
            results = []
            for segment in segments:
                text = segment.text.strip()
                if text:
                    results.append({
                        'start': segment.start,
                        'end': segment.end,
                        'text': text,
                        'is_final': True
                    })
            
            return results
        except Exception as e:
            print(f"Transcription error: {e}")
            return []
    
    def transcribe_chunk_async(self, audio_data):
        """Асинхронное распознавание"""
        try:
            if not self.transcription_queue.full():
                self.transcription_queue.put_nowait(audio_data)
            else:
                print("  Warning: transcription queue full, skipping chunk")
        except queue.Full:
            print("  Warning: transcription queue full")
    
    def process_audio_chunk(self, audio_data):
        """
        ОПТИМИЗИРОВАННАЯ обработка чанка
        Копирует логику из рабочего Python кода
        """
        has_speech = self.detect_speech(audio_data)
        
        if has_speech:
            if not self.is_speaking:
                self.is_speaking = True
                self.silence_chunks = 0
                self.send_event({
                    'event': 'speech_start',
                    'timestamp': time.time()
                })
            
            # КЛЮЧ: Добавляем чанк в буфер
            self.audio_buffer.append(audio_data)
            
        elif self.is_speaking:
            # КЛЮЧ: Продолжаем добавлять чанки тишины!
            # Это сохраняет окончание фразы
            self.audio_buffer.append(audio_data)
            self.silence_chunks += 1
            
            # Если тишина достаточно долгая - распознаем
            if self.silence_chunks >= self.silence_threshold:
                self.is_speaking = False
                self.silence_chunks = 0
                
                # Объединяем накопленный буфер
                if self.audio_buffer:
                    full_audio = np.concatenate(self.audio_buffer)
                    self.audio_buffer = []
                    
                    self.send_event({
                        'event': 'speech_end',
                        'timestamp': time.time()
                    })
                    
                    # АСИНХРОННОЕ распознавание
                    self.transcribe_chunk_async(full_audio)
    
    def send_event(self, data):
        """Отправка события клиенту"""
        try:
            message = json.dumps(data, ensure_ascii=False)
            message_bytes = message.encode('utf-8')
            
            size_bytes = len(message_bytes).to_bytes(4, byteorder='big')
            self.client_socket.sendall(size_bytes + message_bytes)
        except Exception as e:
            print(f"Send error: {e}")
    
    def send_result(self, data):
        """Отправка результата распознавания"""
        self.send_event(data)

    def transcribe_file(self, file_path, language=None):
        """
        Распознавание аудио из файла
        """
        try:
            if language:
                self.language = language

            # Проверка существования файла
            import os
            if not os.path.exists(file_path):
                self.send_event({
                    'event': 'file_error',
                    'error': f'Файл не найден: {file_path}',
                    'timestamp': time.time()
                })
                return

            print(f"  Распознавание файла: {file_path}")

            # Транскрипция файла
            segments, info = self.whisper.transcribe(
                file_path,
                language=self.language,
                beam_size=5,
                best_of=5,
                temperature=0.0,
                vad_filter=True,
                vad_parameters=dict(
                    threshold=0.3,
                    min_speech_duration_ms=250,
                    min_silence_duration_ms=100
                ),
                word_timestamps=False
            )

            results = []
            for segment in segments:
                text = segment.text.strip()
                if text:
                    results.append({
                        'start': segment.start,
                        'end': segment.end,
                        'text': text,
                        'is_final': True
                    })

            self.send_event({
                'event': 'file_transcription_complete',
                'results': results,
                'language': info.language,
                'file_path': file_path,
                'timestamp': time.time()
            })

            print(f"  Файл распознан: {len(results)} сегментов")

        except Exception as e:
            print(f"File transcription error: {e}")
            self.send_event({
                'event': 'file_error',
                'error': str(e),
                'timestamp': time.time()
            })

    def cleanup(self):
        """Очистка ресурсов сессии"""
        self.transcription_queue.put(None)
        if self.transcription_thread.is_alive():
            self.transcription_thread.join(timeout=2.0)


class StreamingSTTServer:
    """TCP сервер с потоковым распознаванием"""
    
    def __init__(self, host='127.0.0.1', port=5555, model_path="base"):
        self.host = host
        self.port = port
        self.server_socket = None
        self.running = False
        self.clients = {}
        
        # Определение устройства
        self.device = self._select_device()
        self.vad_device = self.device
        
        # Загрузка VAD
        print("\n  Загрузка Silero VAD...")
        try:
            self.vad_model, utils = torch.hub.load(
                repo_or_dir='snakers4/silero-vad',
                model='silero_vad',
                force_reload=False,
                trust_repo=True,
                verbose=False
            )
            
            self.vad_model.eval()
            
            if self.vad_device == "cuda":
                self.vad_model = self.vad_model.cuda()
                print(f"   VAD загружен на GPU")
            else:
                self.vad_model = self.vad_model.cpu()
                print(f"   VAD загружен на CPU")
                
        except Exception as e:
            print(f"  Ошибка загрузки VAD: {e}")
            print("  Будет использоваться упрощенное определение речи")
            self.vad_model = None
        
        # Загрузка модели Whisper
        print(f"\n  Загрузка модели Whisper '{model_path}' на {self.device.upper()}...")
        
        try:
            if self.device == "cuda":
                compute_type = "float16"
            else:
                compute_type = "int8"
            
            self.whisper = WhisperModel(
                model_path,
                device=self.device,
                compute_type=compute_type,
                num_workers=4,
                download_root=None
            )
            print(f"  Whisper загружен с compute_type: {compute_type}")
            print(" Все модели загружены успешно!\n")
            
        except Exception as e:
            print(f"ОШИБКА загрузки Whisper: {e}")
            raise
    
    def _select_compute_type(self):
        """Автоматический выбор compute_type"""
        if self.device == "cuda":
            try:
                capability = torch.cuda.get_device_capability(0)
                major, minor = capability
                
                print(f"  GPU Compute Capability: {major}.{minor}")
                
                if major >= 7 and minor >= 5:
                    print(f"  Используется int8_float16")
                    return "int8_float16"
                elif major >= 7:
                    print(f"  Используется int8")
                    return "int8"
                else:
                    print(f"  Используется float16")
                    return "float16"
                    
            except Exception as e:
                print(f"  Ошибка определения compute capability: {e}")
                print(f"  Используется float16 (безопасный вариант)")
                return "float16"
        else:
            print(f"  Используется int8 для CPU")
            return "int8"
    
    def _select_device(self):
        """Выбор устройства для вычислений"""
        try:
            if torch.cuda.is_available():
                print(f"  Обнаружен GPU: {torch.cuda.get_device_name(0)}")
                print(f"  CUDA версия: {torch.version.cuda}")
                print(f"  Доступная память: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.2f} GB")
                return "cuda"
            else:
                print("  CUDA не обнаружена (torch собран без GPU) — использую CPU")
                return "cpu"
        except:
            print("  Используется CPU")
            return "cpu"
    
    def handle_client(self, client_socket, address):
        """Обработка подключения клиента"""
        print(f"Клиент подключен: {address}")
        
        session = StreamingSession(
            client_socket,
            self.whisper,
            self.vad_model,
            sample_rate=16000,
            vad_device=self.vad_device
        )
        self.clients[client_socket] = session
        
        try:
            while self.running:
                size_data = client_socket.recv(4)
                if not size_data:
                    break
                
                message_size = int.from_bytes(size_data, byteorder='big')
                
                data = b''
                while len(data) < message_size:
                    chunk = client_socket.recv(min(4096, message_size - len(data)))
                    if not chunk:
                        break
                    data += chunk
                
                if not data:
                    break
                
                try:
                    message = json.loads(data.decode('utf-8'))
                    self.process_message(message, session)
                    
                except json.JSONDecodeError as e:
                    print(f"JSON error: {e}")
                    
        except Exception as e:
            print(f"Client error {address}: {e}")
        finally:
            session.cleanup()
            client_socket.close()
            if client_socket in self.clients:
                del self.clients[client_socket]
            print(f"Клиент отключен: {address}")
    
    def process_message(self, message, session):
        """Обработка сообщения от клиента"""
        command = message.get('command')
        
        if command == 'start_stream':
            session.language = message.get('language', 'ru')
            session.sample_rate = message.get('sample_rate', 16000)
            session.running = True
            
            print(f"  Начата потоковая сессия (язык: {session.language}, SR: {session.sample_rate})")
            
            session.send_event({
                'event': 'stream_started',
                'status': 'ok'
            })
        
        elif command == 'stop_stream':
            session.running = False
            
            if session.audio_buffer:
                full_audio = np.concatenate(session.audio_buffer)
                session.audio_buffer = []
                session.transcribe_chunk_async(full_audio)
            
            print("  Остановлена потоковая сессия")
            
            session.send_event({
                'event': 'stream_stopped',
                'status': 'ok'
            })
        
        elif command == 'audio_chunk':
            if not session.running:
                return
            
            audio_base64 = message.get('audio_data')
            if not audio_base64:
                return
            
            try:
                audio_bytes = base64.b64decode(audio_base64)
                audio_data = np.frombuffer(audio_bytes, dtype=np.int16)
                
                session.process_audio_chunk(audio_data)
                
            except Exception as e:
                print(f"  Audio processing error: {e}")
        
        elif command == 'ping':
            session.send_event({
                'event': 'pong',
                'status': 'ok'
            })
        
        elif command == 'get_status':
            session.send_event({
                'event': 'status',
                'device': self.device,
                'vad_loaded': self.vad_model is not None,
                'vad_device': self.vad_device,
                'clients_count': len(self.clients),
                'streaming': session.running
            })

        elif command == 'transcribe_file':
            file_path = message.get('file_path')
            language = message.get('language')
            
            if not file_path:
                session.send_event({
                    'event': 'file_error',
                    'error': 'Не указан путь к файлу',
                    'timestamp': time.time()
                })
                return
            
            # Запускаем в отдельном потоке чтобы не блокировать
            thread = threading.Thread(
                target=session.transcribe_file,
                args=(file_path, language),
                daemon=True
            )
            thread.start()
    
    def start(self):
        """Запуск сервера"""
        self.running = True
        
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(5)
            

            while self.running:
                try:
                    client_socket, address = self.server_socket.accept()
                    
                    client_thread = threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, address),
                        daemon=True
                    )
                    client_thread.start()
                    
                except Exception as e:
                    if self.running:
                        print(f"Connection error: {e}")
                    
        except KeyboardInterrupt:
            print("\n\nОстановка сервера...")
        except Exception as e:
            print(f"\nServer error: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Остановка сервера"""
        print("Остановка сервера...")
        self.running = False
        
        for client_socket, session in list(self.clients.items()):
            try:
                session.cleanup()
                client_socket.close()
            except:
                pass
        
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
        
        print("Сервер остановлен")


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='УЛУЧШЕННЫЙ Потоковый STT Server')
    parser.add_argument('--host', default='127.0.0.1', help='IP адрес')
    parser.add_argument('--port', type=int, default=5555, help='Порт')
    parser.add_argument('--model', default='turbo', help='Модель Whisper')
    
    args = parser.parse_args()
    
    try:
        # Путь к модели — относительно скрипта: <speech_scripts>/models/STT-Base
        _script_dir = os.path.dirname(os.path.abspath(__file__))
        _default_model = os.path.join(_script_dir, "..", "models", "STT-Base")

        server = StreamingSTTServer(
            host=args.host,
            port=args.port,
            model_path=_default_model
        )
        
        server.start()
    except Exception as e:
        print(f"\nКритическая ошибка: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)