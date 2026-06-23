# Runtime Dependencies

The translation transcript feature can run text chat translation with only Windows system libraries.

Voice transcription expects the local `whisper.cpp` runtime files under:

```text
dependencies\whisper\Release\whisper-cli.exe
dependencies\whisper\Release\whisper.dll
dependencies\whisper\models\ggml-tiny.bin
```

The current verified source is the official `ggml-org/whisper.cpp` GitHub release asset `whisper-bin-x64.zip` plus `ggml-tiny.bin` from the `ggerganov/whisper.cpp` Hugging Face model repository.

The large runtime files are intentionally ignored by git. The overlay discovers them at runtime from either the repository root or from two directories above the built DLL (`x64\Release`).
