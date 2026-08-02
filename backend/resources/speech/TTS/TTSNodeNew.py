#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESpeech-TTS JSON-line worker as TCP server.
Protocol: [4 bytes big-endian length][JSON data]
"""

import os
import sys
import json
import base64
import io
import wave
import gc
import tempfile
import traceback
import time
import socket
import struct
import threading
from typing import Any, Dict

import numpy as np
import soundfile as sf
import torch
import torchaudio
import warnings

#Scripts\Activate.ps1
# Suppress ffmpeg warning since we use soundfile instead
warnings.filterwarnings("ignore", message="Couldn't find ffmpeg or avconv")

# Патч torchaudio.load для обхода torchcodec
def _patched_load(filepath, *args, **kwargs):
    audio, sr = sf.read(filepath, dtype='float32')
    if audio.ndim == 1:
        audio = audio.reshape(1, -1)
    elif audio.ndim == 2:
        audio = audio.T
    return torch.from_numpy(audio), sr

torchaudio.load = _patched_load

from huggingface_hub import hf_hub_download, snapshot_download
from ruaccent import RUAccent
from f5_tts.infer.utils_infer import (
    infer_process,
    load_model,
    load_vocoder,
    preprocess_ref_audio_text,
    remove_silence_for_generated_wav,
    save_spectrogram,
    tempfile_kwargs,
)
from f5_tts.model import DiT

# Number to words conversion - try multiple libraries
try:
    from number_to_words import process_numbers_in_text
except ImportError:
    try:
        from number_to_words.number_to_words import process_numbers_in_text
    except ImportError:
        try:
            from num2words import num2words
            import re
            
            def process_numbers_in_text(text: str) -> str:
                def replace_number(match):
                    try:
                        num = match.group(0)
                        num = num.replace(' ', '')
                        if '.' in num or ',' in num:
                            num_val = float(num.replace(',', '.'))
                        else:
                            num_val = int(num)
                        return num2words(num_val, lang='ru')
                    except:
                        return match.group(0)
                text = re.sub(r'\b\d+[.,]?\d*\b', replace_number, text)
                return text
        except ImportError:
            def process_numbers_in_text(text: str) -> str:
                return text
            print("Warning: number-to-words libraries not found.", file=sys.stderr)

# ---------- Model Configuration ----------
MODEL_CFG = dict(dim=1024, depth=22, heads=16, ff_mult=2, text_dim=512, conv_layers=4)
MODEL_REPO = "ESpeech/ESpeech-TTS-1_RL-V2"
MODEL_FILE = "espeech_tts_rlv2.pt"
VOCAB_FILE = "vocab.txt"

# Default reference voice
DEFAULT_REF_AUDIO = "D:\\vs\\Project421\\1_projectGPT\\new13\\scrypts\\recorded_voices\\voice_20251025_123518.wav"
DEFAULT_REF_TEXT = "Д+обрый д+ень, мо+ё +имя С+аша и мн+е шестн+адцать л+ет. Ком+анда заинтересов+ала мен+я р+овно в т+оже вр+емя как я откр+ыла для себ+я в+аши перев+оды. "

loaded_model = None
accentizer = None
vocoder = None

# ---------- Global lock for synthesis (model is not thread-safe) ----------
synthesis_lock = threading.Lock()

# ---------- Helpers ----------
def _play_audio(audio: np.ndarray, sr: int) -> None:
    # В серверной версии воспроизведение не используется (оставлено для совместимости, но не вызывается)
    pass

def _encode_wav_b64(audio: np.ndarray, sr: int) -> str:
    data = np.clip(audio, -1.0, 1.0)
    data_i16 = (data * 32767.0).astype("<i2")
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(data_i16.tobytes())
    return base64.b64encode(buf.getvalue()).decode("ascii")

def _save_audio_file(audio: np.ndarray, sr: int, output_dir: str = None, filename: str = None) -> str:
    if output_dir is None:
        output_dir = tempfile.gettempdir()
    if filename is None:
        timestamp = int(time.time() * 1000)
        filename = f"tts_output_{timestamp}.wav"
    os.makedirs(output_dir, exist_ok=True)
    filepath = os.path.join(output_dir, filename)
    sf.write(filepath, audio, sr)
    return filepath

# ---------- Model Loading ----------
def _get_quantization_dtype():
    quant = CFG.get("quantization", "auto")
    if quant == "none" or quant == "fp32":
        return torch.float32
    if not torch.cuda.is_available():
        return torch.float32
    capability = torch.cuda.get_device_capability()
    major, minor = capability
    # ... (логика выбора типа) ...
    return torch.float32

def _load_model():
    global loaded_model
    if loaded_model is not None:
        return loaded_model
    model_path = None
    vocab_path = None
    try:
        model_path = hf_hub_download(repo_id=MODEL_REPO, filename=MODEL_FILE)
        vocab_path = hf_hub_download(repo_id=MODEL_REPO, filename=VOCAB_FILE)
    except Exception as e:
        print(f"hf_hub_download failed: {e}", file=sys.stderr)
    if model_path is None or vocab_path is None:
        try:
            local_dir = f"cache_{MODEL_REPO.replace('/', '_')}"
            snapshot_dir = snapshot_download(repo_id=MODEL_REPO, cache_dir=None, local_dir=local_dir)
            possible_model = os.path.join(snapshot_dir, MODEL_FILE)
            possible_vocab = os.path.join(snapshot_dir, VOCAB_FILE)
            if os.path.exists(possible_model):
                model_path = possible_model
            if os.path.exists(possible_vocab):
                vocab_path = possible_vocab
        except Exception as e:
            print(f"snapshot_download failed: {e}", file=sys.stderr)
    if not model_path or not os.path.exists(model_path):
        raise FileNotFoundError(f"Model file not found: {model_path}")
    if not vocab_path or not os.path.exists(vocab_path):
        raise FileNotFoundError(f"Vocab file not found: {vocab_path}")
    loaded_model = load_model(DiT, MODEL_CFG, model_path, vocab_file=vocab_path)
    dtype = _get_quantization_dtype()
    if dtype != torch.float32:
        loaded_model = loaded_model.to(dtype)
    return loaded_model

def _find_default_ref_audio():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        DEFAULT_REF_AUDIO,
        os.path.join(script_dir, DEFAULT_REF_AUDIO),
        os.path.join(script_dir, "voices", DEFAULT_REF_AUDIO),
        os.path.join(script_dir, "ref", DEFAULT_REF_AUDIO),
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return None

def _init_components():
    global accentizer, vocoder, CFG
    try:
        _load_model()
    except Exception as e:
        print(f"Model load failed: {e}", file=sys.stderr)
        sys.exit(1)
    accentizer = RUAccent()
    accentizer.load(omograph_model_size='turbo3', use_dictionary=True, tiny_mode=False)
    vocoder = load_vocoder()
    default_ref = _find_default_ref_audio()
    if default_ref:
        CFG["ref_audio"] = default_ref
        CFG["ref_text"] = DEFAULT_REF_TEXT
        print(f"Default reference loaded: {default_ref}", file=sys.stderr)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"TTS Server ready, device: {device}", file=sys.stderr)

# ---------- Text Processing ----------
def _process_text_with_accent(text: str) -> str:
    #return text
    if not text or not text.strip():
        return text
    text = process_numbers_in_text(text)
    if '+' in text:
        return text
    return accentizer.process_all(text)

import re
def _split_sentences(text: str):
    return re.split(r'([.!?]+)', text)

def _count_words(text: str) -> int:
    return len(text.split())

# ---------- Synthesis (with callbacks) ----------
def _synthesize(
    ref_audio: str,
    ref_text: str,
    gen_text: str,
    params: Dict[str, Any],
    output_callback,
    check_interrupt
) -> None:
    seed = int(params.get("seed", -1))
    speed = float(params.get("speed", 1.0))
    nfe_step = int(params.get("nfe_step", 16))
    cross_fade_duration = float(params.get("cross_fade_duration", 0.15))
    remove_silence = bool(params.get("remove_silence", False))
    return_b64 = bool(params.get("return_b64", False))
    do_play = bool(params.get("play", True))          # игнорируется на сервере
    save_file = bool(params.get("save_file", False))
    output_dir = params.get("output_dir", None)
    
    global vocoder, loaded_model

    if seed < 0 or seed > 2**31 - 1:
        seed = np.random.randint(0, 2**31 - 1)
    torch.manual_seed(seed)
    
    if not ref_audio or not os.path.exists(ref_audio):
        output_callback({"type":"error","text":"Reference audio not found"})
        return
    if not gen_text or not gen_text.strip():
        output_callback({"type":"error","text":"Generation text is empty"})
        return
    if not ref_text or not ref_text.strip():
        output_callback({"type":"error","text":"Reference text is empty"})
        return
    
    processed_ref_text = _process_text_with_accent(ref_text)
    processed_gen_text = _process_text_with_accent(gen_text)
    
    if check_interrupt():
        output_callback({"type":"log","text":"synthesis cancelled before start"})
        return
    
    try:
        model = _load_model()
    except Exception as e:
        output_callback({"type":"error","text":f"Model load failed: {e}"})
        return
    
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    dtype = _get_quantization_dtype()
    
    try:
        if device.type == "cuda":
            model.to(device)
            if dtype != torch.float32:
                model = model.to(dtype)
            vocoder.to(device)
            if dtype != torch.float32:
                vocoder = vocoder.to(dtype)
        
        if check_interrupt():
            output_callback({"type":"log","text":"synthesis interrupted before preprocess"})
            return
        
        ref_audio_proc, processed_ref_text_final = preprocess_ref_audio_text(
            ref_audio,
            processed_ref_text,
            show_info=lambda x: output_callback({"type":"log","text":x})
        )
        
        if check_interrupt():
            output_callback({"type":"log","text":"synthesis interrupted before infer"})
            return
        
        final_wave, final_sample_rate, combined_spectrogram = infer_process(
            ref_audio_proc,
            processed_ref_text_final,
            processed_gen_text,
            model,
            vocoder,
            cross_fade_duration=cross_fade_duration,
            nfe_step=nfe_step,
            speed=speed,
            show_info=lambda x: output_callback({"type":"log","text":x}),
            progress=None,
        )
        
        if check_interrupt():
            output_callback({"type":"log","text":"synthesis interrupted after infer"})
            return
        
        if remove_silence:
            try:
                with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
                    temp_path = f.name
                sf.write(temp_path, final_wave, final_sample_rate)
                remove_silence_for_generated_wav(temp_path)
                final_wave, _ = sf.read(temp_path)
                os.unlink(temp_path)
            except Exception as e:
                output_callback({"type":"log","text":f"Remove silence failed: {e}"})
        
        if check_interrupt():
            output_callback({"type":"log","text":"synthesis interrupted before output"})
            return
        
        saved_file_path = None
        if save_file:
            saved_file_path = _save_audio_file(final_wave, final_sample_rate, output_dir)
            if saved_file_path:
                output_callback({
                    "type":"audio_file",
                    "path": saved_file_path,
                    "sample_rate": final_sample_rate,
                    "format":"wav",
                    "channels":1
                })
        
        if return_b64:
            try:
                b64 = _encode_wav_b64(final_wave, final_sample_rate)
                output_callback({
                    "type":"audio",
                    "audio_b64": b64,
                    "sample_rate": final_sample_rate,
                    "format":"wav",
                    "channels":1
                })
            except Exception as e:
                output_callback({"type":"log","text":f"encode_b64 failed: {e}"})
        
        # Воспроизведение на сервере не делаем (do_play игнорируется)
        
        output_callback({"type":"final","text": processed_gen_text})
    
    except Exception as e:
        output_callback({"type":"error","text":f"Synthesis failed: {e}"})
        traceback.print_exc(file=sys.stderr)
    
    finally:
        if device.type == "cuda":
            try:
                model.to("cpu")
                vocoder.to("cpu")
                torch.cuda.empty_cache()
                gc.collect()
            except Exception:
                pass

# ---------- Global Config ----------
CFG = {
    "seed": -1,
    "speed": 1,
    "nfe_step": 32,
    "cross_fade_duration": 0.15,
    "remove_silence": False,
    "return_b64": False,
    "play": False,
    "save_file": True,
    "output_dir": None,
    "ref_audio": None,
    "ref_text": None,
    "quantization": "auto",
}

# ---------- Session Class ----------
class TTSSession:
    def __init__(self, client_socket, addr):
        self.sock = client_socket
        self.addr = addr
        self.running = True
        self.interrupted = False
        self.stream_buffer = ""
        self.stream_active = False

    def send_response(self, data):
        try:
            json_str = json.dumps(data, ensure_ascii=False)
            msg = json_str.encode('utf-8')
            self.sock.sendall(struct.pack('>I', len(msg)) + msg)
        except Exception as e:
            print(f"Error sending response to {self.addr}: {e}", file=sys.stderr)

    def run(self):
        while self.running:
            try:
                header = self.sock.recv(4)
                if not header:
                    break
                msg_len = struct.unpack('>I', header)[0]
                data = b''
                while len(data) < msg_len:
                    chunk = self.sock.recv(min(4096, msg_len - len(data)))
                    if not chunk:
                        break
                    data += chunk
                if len(data) != msg_len:
                    break
                req = json.loads(data.decode('utf-8'))
                self.handle_request(req)
            except (ConnectionResetError, BrokenPipeError):
                break
            except Exception as e:
                print(f"Session error {self.addr}: {e}", file=sys.stderr)
                traceback.print_exc(file=sys.stderr)
                break
        self.sock.close()
        print(f"Client {self.addr} disconnected", file=sys.stderr)

    def handle_request(self, req):
        event = req.get("event", "").lower()
        if event == "input":
            self.on_input(req)
        elif event == "config":
            self.on_config(req)
        elif event == "reset":
            self.on_reset(req)
        elif event == "interrupt":
            self.on_interrupt(req)
        elif event == "stream_start":
            self.on_stream_start(req)
        elif event == "stream_chunk":
            self.on_stream_chunk(req)
        elif event == "stream_end":
            self.on_stream_end(req)
        else:
            self.send_response({"type":"log","text":f"unknown event: {event}"})

    def on_input(self, req):
        payload = req.get("payload", {})
        params = req.get("params", {}) or req.get("param", {}) or {}
        # Извлечение текста (как в оригинальном _on_input)
        gen_text = ""
        ref_audio = params.get("ref_audio") or CFG.get("ref_audio") or ""
        ref_text = params.get("ref_text") or CFG.get("ref_text") or ""

        if isinstance(payload, dict):
            if "type" in payload and "payload" in payload:
                inner = payload.get("payload", "")
                if isinstance(inner, str):
                    gen_text = inner
                elif isinstance(inner, dict):
                    gen_text = inner.get("text", "") or str(inner)
                else:
                    gen_text = str(inner)
            else:
                gen_text = (payload.get("gen_text") or payload.get("text") or
                            payload.get("content") or payload.get("data") or str(payload.get("payload", "")))
        elif isinstance(payload, str):
            gen_text = payload
        else:
            gen_text = str(payload) if payload else ""

        if not gen_text or not gen_text.strip():
            self.send_response({"type":"error","text":"No generation text provided"})
            return

        merged_params = {**CFG, **params}
        self.send_response({"type":"log","text":f"Synthesizing: {gen_text[:100]}..."})

        # Запускаем синтез в отдельном потоке с глобальной блокировкой
        thread = threading.Thread(target=self._do_synthesis,
                                   args=(ref_audio, ref_text, gen_text, merged_params))
        thread.daemon = True
        thread.start()

    def _do_synthesis(self, ref_audio, ref_text, gen_text, params):
        with synthesis_lock:
            self.interrupted = False
            _synthesize(ref_audio, ref_text, gen_text, params,
                        output_callback=self.send_response,
                        check_interrupt=lambda: self.interrupted)

    def on_config(self, req):
        params = req.get("params", {}) or {}
        # Обновляем глобальный CFG под блокировкой (можно later хранить в сессии)
        with synthesis_lock:
            for k, v in params.items():
                if k in CFG:
                    CFG[k] = v
        self.send_response({"type":"log","text":"config updated"})

    def on_reset(self, req):
        self.stream_buffer = ""
        self.stream_active = False
        self.interrupted = False
        self.send_response({"type":"log","text":"reset"})

    def on_interrupt(self, req):
        self.interrupted = True
        self.stream_buffer = ""
        self.stream_active = False
        self.send_response({"type":"log","text":"interrupted"})

    def on_stream_start(self, req):
        self.stream_buffer = ""
        self.stream_active = True
        self.send_response({"type":"log","text":"stream started"})

    def on_stream_chunk(self, req):
        if not self.stream_active:
            self.send_response({"type":"log","text":"stream_chunk ignored: stream not active"})
            return
        if self.interrupted:
            self.stream_active = False
            self.stream_buffer = ""
            self.send_response({"type":"log","text":"stream_chunk cancelled by interrupt"})
            return

        payload = req.get("payload", "")
        chunk = ""
        if isinstance(payload, dict):
            chunk = payload.get("text", "") or payload.get("content", "") or payload.get("data", "") or str(payload.get("payload", ""))
        elif isinstance(payload, str):
            chunk = payload
        else:
            chunk = str(payload)

        if not chunk:
            return

        self.stream_buffer += chunk
        parts = _split_sentences(self.stream_buffer)
        sentences_to_process = []
        remaining = ""
        i = 0
        while i < len(parts) - 1:
            text_part = parts[i]
            delimiter = parts[i+1] if i+1 < len(parts) else ""
            if delimiter:
                sentence = text_part + delimiter
                if _count_words(sentence) >= 3:
                    sentences_to_process.append(sentence.strip())
                else:
                    remaining += sentence
                i += 2
            else:
                remaining += text_part
                i += 1
        if i < len(parts):
            remaining += parts[-1]

        for sentence in sentences_to_process:
            ref_audio = CFG.get("ref_audio") or ""
            ref_text = CFG.get("ref_text") or ""
            if ref_audio and ref_text and sentence:
                thread = threading.Thread(target=self._do_synthesis,
                                           args=(ref_audio, ref_text, sentence, CFG.copy()))
                thread.daemon = True
                thread.start()

        self.stream_buffer = remaining

    def on_stream_end(self, req):
        if self.stream_active and self.stream_buffer.strip():
            ref_audio = CFG.get("ref_audio") or ""
            ref_text = CFG.get("ref_text") or ""
            if ref_audio and ref_text:
                thread = threading.Thread(target=self._do_synthesis,
                                           args=(ref_audio, ref_text, self.stream_buffer.strip(), CFG.copy()))
                thread.daemon = True
                thread.start()
        self.stream_buffer = ""
        self.stream_active = False
        self.send_response({"type":"log","text":"stream ended"})

# ---------- Server Class ----------
class TTSServer:
    def __init__(self, host='127.0.0.1', port=5556):
        self.host = host
        self.port = port
        self.sock = None
        self.running = False

    def start(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((self.host, self.port))
        self.sock.listen(5)
        print(f"TTS Server listening on {self.host}:{self.port}", file=sys.stderr)
        self.running = True
        while self.running:
            try:
                client, addr = self.sock.accept()
                print(f"New connection from {addr}", file=sys.stderr)
                session = TTSSession(client, addr)
                thread = threading.Thread(target=session.run)
                thread.daemon = True
                thread.start()
            except Exception as e:
                if self.running:
                    print(f"Accept error: {e}", file=sys.stderr)

    def stop(self):
        self.running = False
        if self.sock:
            self.sock.close()

# ---------- Main ----------
if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description='TTS TCP Server')
    parser.add_argument('--host', default='127.0.0.1', help='Bind host')
    parser.add_argument('--port', type=int, default=5556, help='Bind port')
    args = parser.parse_args()

    _init_components()
    server = TTSServer(args.host, args.port)
    try:
        server.start()
    except KeyboardInterrupt:
        print("\nShutting down...", file=sys.stderr)
        server.stop()