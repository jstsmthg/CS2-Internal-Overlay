#pragma once

namespace hooks {
void Init();
void Cleanup();

// =========================================================================
// Menu
// =========================================================================
inline bool showMenu = true;

// =========================================================================
// Display scaling (set by present.cpp from swap chain vs client rect)
// =========================================================================
inline float scaleX = 1.0f;
inline float scaleY = 1.0f;

// =========================================================================
// Generic key-bind system (used by WndProc to capture any key)
// Set pendingKeyBind to the address of the int to write, and
// pendingKeyBindFlag to the address of the bool "waiting" flag.
// WndProc will write the key code and clear both pointers.
// =========================================================================
inline int *pendingKeyBind = nullptr;
inline bool *pendingKeyBindFlag = nullptr;

// =========================================================================
// Aimbot
// =========================================================================
inline bool aimbotEnabled = false;
inline int aimbotKey = 0xA4; // VK_LMENU (Left Alt)
inline bool waitingForAimbotKey = false;
inline float aimbotSmoothing = 1.0f;
inline float aimbotFov = 15.0f;
inline bool aimbotVisCheck = true;
inline int aimbotHitbox = 0; // 0=head,1=neck,2=chest,3=pelvis,4=legs
inline bool aimbotClosestHitbox = false;
inline bool aimbotLinearSmooth = false;
inline bool aimbotShowFov = false;
inline float aimbotFovColor[4] = {0.f, 0.83f, 1.f, 1.f};
inline bool aimbotFovFollowRecoil = false;
inline float aimbotFovFollowColor[4] = {1.f, 0.f, 0.f, 1.f};
inline int aimbotWeaponCat = 0; // 0=All,1=Rifle,2=SMG,3=Pistol,4=Sniper,5=Shotgun

// =========================================================================
// RCS
// =========================================================================
inline bool rcsEnabled = false;
inline float rcsX = 100.0f;
inline float rcsY = 100.0f;

// =========================================================================
// Triggerbot
// =========================================================================
inline bool triggerbotEnabled = false;
inline int triggerbotDelay = 0;
inline bool triggerbotAim = false;
inline int triggerbotAimKey = 0xA4;
inline bool waitingForTriggerAimKey = false;
inline bool triggerbotAimUseRecoil = false;
inline float triggerbotAimFov = 15.0f;
inline float triggerbotAimSmooth = 1.0f;
inline int triggerbotAimHitbox = 0;

// =========================================================================
// Visuals / ESP
// =========================================================================
inline bool espEnabled = false;
inline int espStyle = 0;       // 0=2D Box, 1=Cornered, 2=3D Glow
inline bool espNameEnabled = true;
inline float espNameColor[4] = {1.f, 1.f, 1.f, 1.f};
inline float espBoxColor[4] = {1.f, 0.f, 0.f, 1.f};
inline float espCorneredColor[4] = {0.f, 0.83f, 1.f, 1.f};
inline bool espHealthBar = true;
inline float espHealthStartColor[4] = {0.f, 1.f, 0.f, 1.f};
inline float espHealthEndColor[4] = {1.f, 0.f, 0.f, 1.f};
inline bool espArmorBar = false;
inline float espArmorColor[4] = {0.f, 0.5f, 1.f, 1.f};
inline bool espWeaponEnabled = false;
inline float espWeaponColor[4] = {0.8f, 0.8f, 0.8f, 1.f};
inline bool espSkeletonEnabled = false;
inline float espSkeletonColor[4] = {1.f, 1.f, 1.f, 0.8f};
inline bool espHeadCircle = false;
inline float espHeadCircleColor[4] = {1.f, 0.f, 0.f, 1.f};
inline bool espWeaponIcon = false;
inline bool espSoundEnabled = false;
inline float espSoundTime = 2.0f;
inline int espToggleKey = 0;
inline bool waitingForEspKey = false;

// =========================================================================
// Radar
// =========================================================================
inline bool radarEnabled = false;
inline bool radarForceHud = false;
inline bool radarOutlines = true;
inline bool radarPlayerDot = true;
inline float radarDotColor[4] = {1.f, 0.f, 0.f, 1.f};
inline bool radarDroppedWeapons = false;
inline bool radarDroppedGrenades = false;

// =========================================================================
// World ESP
// =========================================================================
inline bool bombTimerEnabled = true;
inline bool bombLocation = true;
inline float bombLocationColor[4] = {1.f, 0.f, 0.f, 1.f};
inline bool bombDefuseCircle = true;
inline bool droppedItemsEnabled = false;
inline float droppedItemsColor[4] = {0.8f, 0.8f, 0.2f, 1.f};
inline bool droppedWeaponsEnabled = false;
inline bool droppedGrenadesEnabled = false;
inline bool molotovFireEnabled = false;

// =========================================================================
// Misc
// =========================================================================
inline bool ignoreTeam = false;
inline bool streamproof = false;
inline float customGameFov = 0.0f; // 0 = default
inline bool spectatorList = false;
inline bool spectatorListVerbose = false;
inline bool hitmarkerEnabled = false;
inline int hitmarkerSound = 0; // 0=none,1=cod,2=bell,3=minecraft
inline float hitmarkerVolume = 50.0f;
inline bool watermarkEnabled = true;
inline int menuKeyBind = VK_INSERT;
inline bool waitingForMenuKey = false;
inline bool recoilCrosshair = false;
inline bool snaptapEnabled = false;
inline int maxFps = 0;    // 0 = uncapped
inline bool vsyncEnabled = false;

// =========================================================================
// Grenade Helper
// =========================================================================
inline bool grenadeHelperEnabled = false;
inline int grenadeFilter = 0;    // 0=all,1=smoke,2=flash,3=molly,4=HE
inline float grenadeDistance = 500.0f;
inline float grenadeColor[4] = {0.f, 1.f, 0.f, 1.f};
inline bool grenadeAimAssist = false;

} // namespace hooks
