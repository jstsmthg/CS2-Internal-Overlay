#pragma once

namespace transcript {
void Init();
void Shutdown();
void OnFrame();
void Render();
void AddVoiceText(const char *text);
const char *Status();
}
