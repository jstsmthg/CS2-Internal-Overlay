#pragma once

namespace hooks {
void Init();
void Cleanup();

inline bool showMenu = true;
inline bool espEnabled = false;
inline int espStyle = 1; // 0 = 2D Boxes, 1 = 3D Glow (Silhouettes)
inline bool aimbotEnabled = false;
inline bool triggerbotEnabled = false;
inline bool radarEnabled = false;
inline bool rcsEnabled = false;
inline bool bombTimerEnabled = true;

inline bool ignoreTeam = false; // Deathmatch/FFA target everyone

inline float scaleX = 1.0f;
inline float scaleY = 1.0f;

// Aimbot settings
inline int aimbotKey = 0xA4; // VK_LMENU (Left Alt)
inline bool waitingForAimbotKey = false;
inline float aimbotSmoothing = 1.0f;
inline float aimbotFov = 15.0f;
inline bool aimbotVisCheck = true; // Only lock visible players

// Triggerbot settings
inline int triggerbotDelay = 0; // ms

} // namespace hooks
