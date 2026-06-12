#include "pch.h"
#include <algorithm>

namespace {

static bool g_streamproofApplied = false;
static bool g_lastStreamproofState = false;
static ULONGLONG g_lastHitmarkerTick = 0;
static int g_lastHealth[65] = {};
static ULONGLONG g_lastFrameLimiterTick = 0;
static bool g_prevA = false, g_prevD = false, g_prevW = false, g_prevS = false;
static ULONGLONG g_lastPressA = 0, g_lastPressD = 0, g_lastPressW = 0, g_lastPressS = 0;
static ULONGLONG g_brakeUntilLeft = 0, g_brakeUntilRight = 0;
static ULONGLONG g_brakeUntilForward = 0, g_brakeUntilBack = 0;

static uintptr_t ResolveIndex(uintptr_t entityList, int index) {
  uintptr_t listEntry =
      *(uintptr_t *)(entityList + 0x10 + 8 * ((index & 0x7FFF) >> 9));
  if (!listEntry)
    return 0;
  return *(uintptr_t *)(listEntry + 0x70 * (index & 0x1FF));
}

static uintptr_t ResolveHandle(uintptr_t entityList, uint32_t handle) {
  if (!handle || handle == 0xFFFFFFFF)
    return 0;
  uintptr_t listEntry =
      *(uintptr_t *)(entityList + 0x10 + 8 * ((handle & 0x7FFF) >> 9));
  if (!listEntry)
    return 0;
  return *(uintptr_t *)(listEntry + 0x70 * ((handle & 0x7FFF) & 0x1FF));
}

static const char *ObserverModeName(uint8_t mode) {
  switch (mode) {
  case 1: return "deathcam";
  case 2: return "freezecam";
  case 3: return "fixed";
  case 4: return "in-eye";
  case 5: return "chase";
  case 6: return "roaming";
  default: return "spec";
  }
}

static Vector3 GetLatestPunch(uintptr_t localPawn) {

  uintptr_t aimPunchServices =
      *(uintptr_t *)(localPawn + schemas::C_CSPlayerPawn::m_pAimPunchServices);
  if (!aimPunchServices)
    return {};

  uintptr_t cacheData = *(uintptr_t *)(aimPunchServices + 0x70);
  int cacheSize = *(int *)(aimPunchServices + 0x70 + 0x08);
  if (!cacheData || cacheSize <= 0 || cacheSize > 0xFFFF)
    return {};

  return *(Vector3 *)(cacheData + (cacheSize - 1) * sizeof(Vector3));
}

static void SetButton(uintptr_t clientBase, std::ptrdiff_t buttonOffset, bool down) {
  *(int *)(clientBase + buttonOffset) = down ? 65537 : 256;
}

static void ApplySnapTap(uintptr_t clientBase, uintptr_t localPawn) {
  if (!hooks::snaptapEnabled || !localPawn)
    return;

  ULONGLONG now = GetTickCount64();
  bool keyA = (GetAsyncKeyState('A') & 0x8000) != 0;
  bool keyD = (GetAsyncKeyState('D') & 0x8000) != 0;
  bool keyW = (GetAsyncKeyState('W') & 0x8000) != 0;
  bool keyS = (GetAsyncKeyState('S') & 0x8000) != 0;

  if (keyA && !g_prevA) g_lastPressA = now;
  if (keyD && !g_prevD) g_lastPressD = now;
  if (keyW && !g_prevW) g_lastPressW = now;
  if (keyS && !g_prevS) g_lastPressS = now;

  Vector3 velocity = *(Vector3 *)(localPawn + schemas::C_BaseEntity::m_vecAbsVelocity);
  int health = *(int *)(localPawn + schemas::C_BaseEntity::m_iHealth);
  if (health <= 0)
    return;

  Vector3 viewAngles = *(Vector3 *)(localPawn + schemas::C_CSPlayerPawn::m_angEyeAngles);
  float yawRad = viewAngles.y * (3.14159265f / 180.0f);
  Vector3 forward(std::cosf(yawRad), std::sinf(yawRad), 0.0f);
  Vector3 right(-std::sinf(yawRad), std::cosf(yawRad), 0.0f);
  float forwardSpeed = velocity.Dot(forward);
  float sideSpeed = velocity.Dot(right);

  if (!keyA && !keyD) {
    if (!g_prevA && !g_prevD) {
      if (sideSpeed > 25.0f)
        g_brakeUntilLeft = now + 28;
      else if (sideSpeed < -25.0f)
        g_brakeUntilRight = now + 28;
    }
  } else {
    g_brakeUntilLeft = 0;
    g_brakeUntilRight = 0;
  }

  if (!keyW && !keyS) {
    if (!g_prevW && !g_prevS) {
      if (forwardSpeed > 25.0f)
        g_brakeUntilBack = now + 28;
      else if (forwardSpeed < -25.0f)
        g_brakeUntilForward = now + 28;
    }
  } else {
    g_brakeUntilForward = 0;
    g_brakeUntilBack = 0;
  }

  bool holdLeft = false, holdRight = false, holdForward = false, holdBack = false;

  if (keyA && keyD) {
    holdLeft = g_lastPressA >= g_lastPressD;
    holdRight = !holdLeft;
  } else {
    holdLeft = keyA;
    holdRight = keyD;
  }

  if (keyW && keyS) {
    holdForward = g_lastPressW >= g_lastPressS;
    holdBack = !holdForward;
  } else {
    holdForward = keyW;
    holdBack = keyS;
  }

  if (!holdLeft && !holdRight) {
    holdLeft = g_brakeUntilLeft > now;
    holdRight = g_brakeUntilRight > now;
  }
  if (!holdForward && !holdBack) {
    holdForward = g_brakeUntilForward > now;
    holdBack = g_brakeUntilBack > now;
  }

  SetButton(clientBase, buttons::left, holdLeft);
  SetButton(clientBase, buttons::right, holdRight);
  SetButton(clientBase, buttons::forward, holdForward);
  SetButton(clientBase, buttons::back, holdBack);

  g_prevA = keyA;
  g_prevD = keyD;
  g_prevW = keyW;
  g_prevS = keyS;
}

static void ApplyFovOverride(uintptr_t clientBase) {
  if (hooks::customGameFov <= 0.0f)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  uintptr_t localController =
      *(uintptr_t *)(clientBase + offsets::dwLocalPlayerController);
  if (localPawn) {
    *(uint32_t *)(localPawn + schemas::CCSPlayerBase_CameraServices::m_iFOV) =
        (uint32_t)hooks::customGameFov;
  }
  if (localController) {
    *(uint32_t *)(localController +
                  schemas::CBasePlayerController::m_iDesiredFOV) =
        (uint32_t)hooks::customGameFov;
  }
}

static void UpdateHitmarker(uintptr_t clientBase) {
  if (!hooks::hitmarkerEnabled)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!localPawn || !entityList)
    return;

  int localTeam = *(uint8_t *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);
  int shotsFired = *(int *)(localPawn + schemas::C_CSPlayerPawn::m_iShotsFired);

  for (int i = 1; i <= 64; i++) {
    uintptr_t controller = ResolveIndex(entityList, i);
    if (!controller)
      continue;

    uint32_t pawnHandle =
        *(uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
    uintptr_t pawn = ResolveHandle(entityList, pawnHandle);
    if (!pawn || pawn == localPawn)
      continue;

    int team = *(uint8_t *)(pawn + schemas::C_BaseEntity::m_iTeamNum);
    int health = *(int *)(pawn + schemas::C_BaseEntity::m_iHealth);
    if (team == localTeam || health < 0)
      continue;

    int &last = g_lastHealth[i];
    if (last > 0 && health < last && shotsFired > 0) {
      g_lastHitmarkerTick = GetTickCount64();
      if (hooks::hitmarkerSound != 0) {
        switch (hooks::hitmarkerSound) {
        case 1:
          Beep(1700, 20);
          break;
        case 2:
          Beep(1100, 40);
          break;
        case 3:
          Beep(900, 25);
          Sleep(8);
          Beep(1200, 25);
          break;
        default:
          break;
        }
      }
    }
    last = health;
  }
}

// Watermark removed by request

static void RenderRecoilCrosshair(uintptr_t clientBase) {
  if (!hooks::recoilCrosshair)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  Vector3 punch = GetLatestPunch(localPawn);
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  float cx = displaySize.x * 0.5f - (punch.y * 2.0f / 90.0f) * (displaySize.x * 0.5f);
  float cy = displaySize.y * 0.5f + (punch.x * 2.0f / 90.0f) * (displaySize.y * 0.5f);

  ImDrawList *draw = ImGui::GetForegroundDrawList();
  ImU32 outline = IM_COL32(0, 0, 0, 220);
  ImU32 col = IM_COL32(0, 220, 255, 255);
  draw->AddCircleFilled(ImVec2(cx, cy), 2.5f, outline);
  draw->AddCircle(ImVec2(cx, cy), 6.0f, outline, 20, 3.0f);
  draw->AddCircle(ImVec2(cx, cy), 6.0f, col, 20, 1.5f);
  draw->AddLine(ImVec2(cx - 14.0f, cy), ImVec2(cx - 6.0f, cy), outline, 3.0f);
  draw->AddLine(ImVec2(cx + 6.0f, cy), ImVec2(cx + 14.0f, cy), outline, 3.0f);
  draw->AddLine(ImVec2(cx, cy - 14.0f), ImVec2(cx, cy - 6.0f), outline, 3.0f);
  draw->AddLine(ImVec2(cx, cy + 6.0f), ImVec2(cx, cy + 14.0f), outline, 3.0f);
  draw->AddLine(ImVec2(cx - 14.0f, cy), ImVec2(cx - 6.0f, cy), col, 1.5f);
  draw->AddLine(ImVec2(cx + 6.0f, cy), ImVec2(cx + 14.0f, cy), col, 1.5f);
  draw->AddLine(ImVec2(cx, cy - 14.0f), ImVec2(cx, cy - 6.0f), col, 1.5f);
  draw->AddLine(ImVec2(cx, cy + 6.0f), ImVec2(cx, cy + 14.0f), col, 1.5f);
}

static void RenderHitmarker() {
  if (!hooks::hitmarkerEnabled)
    return;

  ULONGLONG now = GetTickCount64();
  ULONGLONG age = now - g_lastHitmarkerTick;
  if (age > 180)
    return;

  float alphaFrac = 1.0f - (float)age / 180.0f;
  int alpha = (int)(255.0f * alphaFrac);
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  float cx = displaySize.x * 0.5f;
  float cy = displaySize.y * 0.5f;
  ImDrawList *draw = ImGui::GetForegroundDrawList();
  ImU32 col = IM_COL32(255, 255, 255, alpha);
  draw->AddLine(ImVec2(cx - 9.0f, cy - 9.0f), ImVec2(cx - 3.0f, cy - 3.0f), col,
                2.0f);
  draw->AddLine(ImVec2(cx + 9.0f, cy - 9.0f), ImVec2(cx + 3.0f, cy - 3.0f), col,
                2.0f);
  draw->AddLine(ImVec2(cx - 9.0f, cy + 9.0f), ImVec2(cx - 3.0f, cy + 3.0f), col,
                2.0f);
  draw->AddLine(ImVec2(cx + 9.0f, cy + 9.0f), ImVec2(cx + 3.0f, cy + 3.0f), col,
                2.0f);
}

static void RenderSpectatorList(uintptr_t clientBase) {
  if (!hooks::spectatorList)
    return;

  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  // Find the pawn we are currently controlling/spectating
  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  // Determine which pawn(s) we consider "the watched target"
  // When alive: watchedPawn = our own player pawn
  // When spectating: watchedPawn = the player pawn of whoever we're watching
  uintptr_t watchedPawn = localPawn;
  uintptr_t localController = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerController);

  __try {
    if (localController) {
      bool localAlive = *(bool *)(localController + schemas::CCSPlayerController::m_bPawnIsAlive);
      if (!localAlive) {
        // We are dead — find who we're spectating via our observer pawn
        uint32_t obsHandle =
            *(uint32_t *)(localController + schemas::CCSPlayerController::m_hObserverPawn);
        uintptr_t obsPawn = ResolveHandle(entityList, obsHandle);
        if (obsPawn) {
          uintptr_t obsServices =
              *(uintptr_t *)(obsPawn + schemas::C_BasePlayerPawn::m_pObserverServices);
          if (obsServices) {
            uint8_t mode =
                *(uint8_t *)(obsServices + schemas::CPlayer_ObserverServices::m_iObserverMode);
            if (mode >= 1 && mode <= 6) {
              uint32_t targetHandle =
                  *(uint32_t *)(obsServices + schemas::CPlayer_ObserverServices::m_hObserverTarget);
              uintptr_t target = ResolveHandle(entityList, targetHandle);
              if (target)
                watchedPawn = target;
            }
          }
        }

        // Also try from the player pawn's observer services as fallback
        if (watchedPawn == localPawn) {
          uintptr_t playerObsServices =
              *(uintptr_t *)(localPawn + schemas::C_BasePlayerPawn::m_pObserverServices);
          if (playerObsServices) {
            uint8_t mode =
                *(uint8_t *)(playerObsServices + schemas::CPlayer_ObserverServices::m_iObserverMode);
            if (mode >= 1 && mode <= 6) {
              uint32_t targetHandle =
                  *(uint32_t *)(playerObsServices + schemas::CPlayer_ObserverServices::m_hObserverTarget);
              uintptr_t target = ResolveHandle(entityList, targetHandle);
              if (target)
                watchedPawn = target;
            }
          }
        }
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {}

  ImGui::SetNextWindowBgAlpha(0.30f);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220.0f, 80.0f),
                          ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("##spectators", nullptr, flags)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Spectators");
  ImGui::Separator();
  int count = 0;

  for (int i = 1; i <= 64; i++) {
    __try {
      uintptr_t controller = ResolveIndex(entityList, i);
      if (!controller)
        continue;

      // Skip alive players — they can't be spectating
      bool isAlive = *(bool *)(controller + schemas::CCSPlayerController::m_bPawnIsAlive);
      if (isAlive)
        continue;

      // Try the observer pawn first (this is the correct pawn for dead/spectating players)
      uintptr_t obsPawn = 0;
      uint32_t obsPawnHandle =
          *(uint32_t *)(controller + schemas::CCSPlayerController::m_hObserverPawn);
      obsPawn = ResolveHandle(entityList, obsPawnHandle);

      // If no observer pawn, try the player pawn (deathcam uses the player pawn)
      if (!obsPawn) {
        uint32_t pawnHandle =
            *(uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
        obsPawn = ResolveHandle(entityList, pawnHandle);
      }

      if (!obsPawn || obsPawn == watchedPawn)
        continue;

      uintptr_t obsServices =
          *(uintptr_t *)(obsPawn + schemas::C_BasePlayerPawn::m_pObserverServices);
      if (!obsServices)
        continue;

      uint8_t obsMode =
          *(uint8_t *)(obsServices + schemas::CPlayer_ObserverServices::m_iObserverMode);
      // Mode 0 = none (not spectating), modes 1-6 are valid observer modes
      if (obsMode == 0)
        continue;

      uint32_t targetHandle =
          *(uint32_t *)(obsServices + schemas::CPlayer_ObserverServices::m_hObserverTarget);
      uintptr_t target = ResolveHandle(entityList, targetHandle);
      if (target != watchedPawn)
        continue;

      uintptr_t namePtr =
          *(uintptr_t *)(controller + schemas::CCSPlayerController::m_sSanitizedPlayerName);
      const char *name = namePtr ? (const char *)namePtr : "unknown";
      if (hooks::spectatorListVerbose) {
        ImGui::Text("%s (%s)", name, ObserverModeName(obsMode));
      } else {
        ImGui::Text("%s", name);
      }
      count++;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      continue;
    }
  }

  if (count == 0)
    ImGui::TextDisabled("No spectators");

  ImGui::End();
}

} // namespace

void misc::OnFrame(HWND gameWindow) {
  if (gameWindow) {
    if (hooks::streamproof != g_lastStreamproofState || !g_streamproofApplied) {
      DWORD affinity = hooks::streamproof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
      SetWindowDisplayAffinity(gameWindow, affinity);
      g_lastStreamproofState = hooks::streamproof;
      g_streamproofApplied = true;
    }
  }

  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  ApplyFovOverride(clientBase);
  UpdateHitmarker(clientBase);
  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  ApplySnapTap(clientBase, localPawn);
}

void misc::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  RenderRecoilCrosshair(clientBase);
  RenderHitmarker();
  RenderSpectatorList(clientBase);
}

void misc::LimitFrameRate() {
  // Max FPS removed by request
}
