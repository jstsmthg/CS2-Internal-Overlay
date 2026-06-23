#pragma once

namespace audio_capture {
using ChunkCallback = std::function<void(const std::vector<float> &)>;

bool Start(ChunkCallback callback);
void Stop();
bool IsRunning();
const char *Status();
}
