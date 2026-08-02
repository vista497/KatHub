# Speech layer (STT + TTS) — перенесено из AI_ControllerApp.
# PocketTTS (pocket_tts.cpp, PocketTTSEngine.cpp) отключён до появления
# onnxruntime в сборке — Python TTS-сервер (TTSNodeNew.py) является
# основным движком, m_useNativeTTS=false по умолчанию.

add_library(kathub-speech STATIC)

target_sources(kathub-speech PRIVATE
    SpeechManager.h
    SpeechManager.cpp
    TTSWrapper.h
    TTSWrapper.cpp
    WhisperWrapper.h
    WhisperWrapper.cpp
    MicrophoneWrapper.h
    MicrophoneWrapper.cpp
    AudioPlayer.h
    AudioPlayer.cpp
    PythonEnvironment.h
    PythonEnvironment.cpp
    # PocketTTS отключён до подключения onnxruntime (FetchContent)
    # PocketTTSEngine.h
    # PocketTTSEngine.cpp
    # pocket_tts.cpp
)

target_include_directories(kathub-speech PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kathub-speech PUBLIC
    kathub-core
    Qt6::Core
    Qt6::Multimedia
    Qt6::Network
)
