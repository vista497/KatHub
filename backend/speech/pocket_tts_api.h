#pragma once
// Выдержка публичного API из pocket_tts.cpp.
// Полные определения — в pocket_tts.cpp (компилируется как статическая библиотека).

#include <string>
#include <vector>
#include <functional>

namespace pocket_tts {

struct Config {
    std::string models_dir = "models";
    std::string tokenizer_path = "models/tokenizer.model";
    std::string voices_dir = "voices";
    std::string precision = "int8";
    float temperature = 0.7f;
    float eos_threshold = -4.0f;
    float noise_clamp = 0.0f;
    int lsd_steps = 1, num_threads = 0, first_chunk_frames = 1, max_chunk_frames = 15;
    int eos_extra_frames = -1;
    bool verbose = false;
    bool voice_cache = true;
};

struct AudioData {
    std::vector<float> samples;
    int sample_rate = 24000;
    float duration_sec() const { return float(samples.size()) / sample_rate; }
};

using StreamCallback = std::function<bool(const float*, size_t)>;

class PocketTTS {
public:
    static constexpr int SR = 24000;

    /// Загрузить аудиофайл (WAV/MP3/FLAC) в PCM float32
    static AudioData load_audio(const std::string& path);

    explicit PocketTTS(const Config& cfg);
    ~PocketTTS();

    // Синтез — возвращает готовый AudioData
    AudioData generate(const std::string& text, const std::string& voice, int max_frames = 500);

    // Стриминг — колбэк на каждый кусок аудио
    void stream(const std::string& text, const std::string& voice, StreamCallback cb, int max_frames = 500);

    const Config& config() const;
};

} // namespace pocket_tts
