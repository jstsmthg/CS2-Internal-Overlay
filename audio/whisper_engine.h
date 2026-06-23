#pragma once

namespace whisper_engine {
bool Init();
void Shutdown();
bool IsReady();
std::string Transcribe(const std::vector<float> &samples, int sampleRate);
const char *Status();
}
