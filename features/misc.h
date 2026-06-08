#pragma once

struct IDXGISwapChain;

namespace misc {
void OnFrame(HWND gameWindow);
void Render();
void LimitFrameRate();
}
