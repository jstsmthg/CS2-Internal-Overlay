#include "pch.h"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {

struct SoundCue {
  Vector3 origin;
  ULONGLONG spawnTime;
};

static std::vector<SoundCue> g_soundCues;
static std::unordered_map<uintptr_t, ULONGLONG> g_lastCueTick;
static bool g_espTogglePressed = false;

static constexpr int kMaxPlayers = 64;
static constexpr int kMaxSoundCues = 64;
static constexpr int kMaxWorldEntities = 2048;

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

static ImU32 ToU32(const float *rgba) {
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

static float Clamp01(float value) {
  return (std::clamp)(value, 0.0f, 1.0f);
}

static ImU32 LerpColor(const float *start, const float *end, float t) {
  t = Clamp01(t);
  ImVec4 col(start[0] * t + end[0] * (1.0f - t),
             start[1] * t + end[1] * (1.0f - t),
             start[2] * t + end[2] * (1.0f - t),
             start[3] * t + end[3] * (1.0f - t));
  return ImGui::ColorConvertFloat4ToU32(col);
}

static bool IsZeroVec(const Vector3 &v) {
  return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

static uint16_t GetItemDefIndex(uintptr_t entity) {
  if (!entity)
    return 0;
  return *(uint16_t *)(entity + schemas::C_EconEntity::m_AttributeManager +
                       schemas::C_AttributeContainer::m_Item +
                       schemas::C_EconItemView::m_iItemDefinitionIndex);
}

static uint16_t GetActiveWeaponDefIndex(uintptr_t pawn, uintptr_t entityList) {
  uintptr_t weaponServices =
      *(uintptr_t *)(pawn + schemas::C_BasePlayerPawn::m_pWeaponServices);
  if (!weaponServices)
    return 0;

  uint32_t handle =
      *(uint32_t *)(weaponServices + schemas::CPlayer_WeaponServices::m_hActiveWeapon);
  return GetItemDefIndex(ResolveHandle(entityList, handle));
}

static const char *WeaponLabel(uint16_t itemDef, bool iconMode) {
  switch (itemDef) {
  case 1: return iconMode ? "DEAG" : "Desert Eagle";
  case 2: return iconMode ? "DUAL" : "Dual Berettas";
  case 3: return iconMode ? "57" : "Five-SeveN";
  case 4: return iconMode ? "GLK" : "Glock-18";
  case 7: return iconMode ? "AK" : "AK-47";
  case 8: return iconMode ? "AUG" : "AUG";
  case 9: return iconMode ? "AWP" : "AWP";
  case 10: return iconMode ? "FMS" : "FAMAS";
  case 11: return iconMode ? "G3" : "G3SG1";
  case 13: return iconMode ? "GAL" : "Galil AR";
  case 14: return iconMode ? "M249" : "M249";
  case 16: return iconMode ? "M4" : "M4A4";
  case 17: return iconMode ? "MAC" : "MAC-10";
  case 19: return iconMode ? "P90" : "P90";
  case 23: return iconMode ? "MP5" : "MP5-SD";
  case 24: return iconMode ? "UMP" : "UMP-45";
  case 25: return iconMode ? "XM" : "XM1014";
  case 26: return iconMode ? "BIZ" : "PP-Bizon";
  case 27: return iconMode ? "MAG" : "MAG-7";
  case 28: return iconMode ? "NEGEV" : "Negev";
  case 29: return iconMode ? "SAW" : "Sawed-Off";
  case 30: return iconMode ? "TEC" : "Tec-9";
  case 32: return iconMode ? "P2K" : "P2000";
  case 33: return iconMode ? "MP7" : "MP7";
  case 34: return iconMode ? "MP9" : "MP9";
  case 35: return iconMode ? "NOVA" : "Nova";
  case 36: return iconMode ? "P250" : "P250";
  case 38: return iconMode ? "SCAR" : "SCAR-20";
  case 39: return iconMode ? "SG" : "SG 553";
  case 40: return iconMode ? "SSG" : "SSG 08";
  case 43: return iconMode ? "FLASH" : "Flashbang";
  case 44: return iconMode ? "HE" : "HE Grenade";
  case 45: return iconMode ? "SMOKE" : "Smoke";
  case 46: return iconMode ? "MOLOTOV" : "Molotov";
  case 47: return iconMode ? "DECOY" : "Decoy";
  case 48: return iconMode ? "INC" : "Incendiary";
  case 49: return iconMode ? "C4" : "C4";
  case 60: return iconMode ? "M4S" : "M4A1-S";
  case 61: return iconMode ? "USP" : "USP-S";
  case 63: return iconMode ? "CZ" : "CZ75-Auto";
  case 64: return iconMode ? "R8" : "R8 Revolver";
  default: return iconMode ? "?" : "Weapon";
  }
}

static void DrawCornerBox(ImDrawList *drawList, float left, float top,
                          float width, float bottom, ImU32 color) {
  float height = bottom - top;
  float cornerLen = (std::min)(width, height) * 0.35f;
  float right = left + width;

  drawList->AddLine(ImVec2(left, top), ImVec2(left + cornerLen, top), color, 2.0f);
  drawList->AddLine(ImVec2(left, top), ImVec2(left, top + cornerLen), color, 2.0f);

  drawList->AddLine(ImVec2(right, top), ImVec2(right - cornerLen, top), color, 2.0f);
  drawList->AddLine(ImVec2(right, top), ImVec2(right, top + cornerLen), color, 2.0f);

  drawList->AddLine(ImVec2(left, bottom), ImVec2(left + cornerLen, bottom), color,
                    2.0f);
  drawList->AddLine(ImVec2(left, bottom), ImVec2(left, bottom - cornerLen), color,
                    2.0f);

  drawList->AddLine(ImVec2(right, bottom), ImVec2(right - cornerLen, bottom), color,
                    2.0f);
  drawList->AddLine(ImVec2(right, bottom), ImVec2(right, bottom - cornerLen), color,
                    2.0f);
}

static void DrawSkeleton(ImDrawList *drawList, uintptr_t boneArray,
                         const view_matrix_t &viewMatrix, float screenW,
                         float screenH, ImU32 color) {
  static const int pairs[][2] = {
      {6, 5}, {5, 4}, {4, 2}, {2, 0}, {4, 8},  {8, 9},  {9, 11},
      {4, 13}, {13, 14}, {14, 16}, {0, 23}, {23, 24}, {0, 26}, {26, 27},
  };

  for (const auto &pair : pairs) {
    Vector3 a = GetBonePosition(boneArray, pair[0]);
    Vector3 b = GetBonePosition(boneArray, pair[1]);
    if (IsZeroVec(a) || IsZeroVec(b))
      continue;

    Vector3 sa, sb;
    if (!WorldToScreen(a, viewMatrix, screenW, screenH, sa) ||
        !WorldToScreen(b, viewMatrix, screenW, screenH, sb)) {
      continue;
    }

    drawList->AddLine(ImVec2(sa.x, sa.y), ImVec2(sb.x, sb.y), color, 1.2f);
  }
}

static void EmitSoundCue(uintptr_t pawn, const Vector3 &origin,
                         const Vector3 &velocity, bool isTeammate) {
  if (!hooks::espSoundEnabled || isTeammate)
    return;

  float speed2D = velocity.Length2D();
  bool noisy = speed2D > 115.0f || std::fabs(velocity.z) > 85.0f;
  if (!noisy)
    return;

  ULONGLONG now = GetTickCount64();
  ULONGLONG &lastTick = g_lastCueTick[pawn];
  if (now - lastTick < 250)
    return;

  lastTick = now;
  g_soundCues.push_back({origin, now});
  if (g_soundCues.size() > kMaxSoundCues)
    g_soundCues.erase(g_soundCues.begin(),
                      g_soundCues.begin() + (g_soundCues.size() - kMaxSoundCues));
}

static void DrawSoundCues(ImDrawList *drawList, const view_matrix_t &viewMatrix,
                          float screenW, float screenH) {
  if (!hooks::espSoundEnabled)
    return;

  ULONGLONG now = GetTickCount64();
  float durationMs = (std::max)(hooks::espSoundTime, 0.1f) * 1000.0f;

  for (size_t i = 0; i < g_soundCues.size();) {
    float age = (float)(now - g_soundCues[i].spawnTime);
    float t = age / durationMs;
    if (t >= 1.0f) {
      g_soundCues.erase(g_soundCues.begin() + i);
      continue;
    }

    Vector3 screen;
    if (WorldToScreen(g_soundCues[i].origin, viewMatrix, screenW, screenH, screen)) {
      float radius = 10.0f + (70.0f * t);
      int alpha = (int)(180.0f * (1.0f - t));
      drawList->AddCircle(ImVec2(screen.x, screen.y), radius,
                          IM_COL32(255, 216, 96, alpha), 32, 2.0f);
    }
    ++i;
  }
}

} // namespace

void esp::OnFrame() {
  if (hooks::espToggleKey == 0 || hooks::pendingKeyBind == &hooks::espToggleKey)
    return;

  bool down = (GetAsyncKeyState(hooks::espToggleKey) & 0x8000) != 0;
  if (down && !g_espTogglePressed)
    hooks::espEnabled = !hooks::espEnabled;
  g_espTogglePressed = down;
}

void esp::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  int localTeam = *(uint8_t *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);

  uintptr_t localSceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  Vector3 localOrigin = {};
  if (localSceneNode) {
    localOrigin =
        *(Vector3 *)(localSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
  }

  view_matrix_t &viewMatrix =
      *(view_matrix_t *)(clientBase + offsets::dwViewMatrix);

  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  float screenW = displaySize.x;
  float screenH = displaySize.y;
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();

  uintptr_t seenPawns[kMaxPlayers] = {};
  int seenCount = 0;

  for (int i = 1; i <= kMaxPlayers; i++) {
    __try {
      uintptr_t controller = ResolveIndex(entityList, i);
      if (!controller)
        continue;

      uint32_t pawnHandle =
          *(uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
      uintptr_t pawn = ResolveHandle(entityList, pawnHandle);
      if (!pawn || pawn == localPawn)
        continue;

      bool duplicate = false;
      for (int s = 0; s < seenCount; s++) {
        if (seenPawns[s] == pawn) {
          duplicate = true;
          break;
        }
      }
      if (duplicate)
        continue;
      if (seenCount < kMaxPlayers)
        seenPawns[seenCount++] = pawn;

      int health = *(int *)(pawn + schemas::C_BaseEntity::m_iHealth);
      if (health <= 0)
        continue;

      int team = *(uint8_t *)(pawn + schemas::C_BaseEntity::m_iTeamNum);
      bool isTeammate = (team == localTeam) && !hooks::ignoreTeam;

      bool isSpotted =
          *(bool *)(pawn + schemas::C_CSPlayerPawn::m_entitySpottedState +
                    schemas::EntitySpottedState_t::m_bSpotted);

      uintptr_t glowProperty = pawn + schemas::C_BaseModelEntity::m_Glow;
      bool *bGlowing =
          (bool *)(glowProperty + schemas::CGlowProperty::m_bGlowing);

      if (hooks::espStyle == 2) {
        int *iGlowType =
            (int *)(glowProperty + schemas::CGlowProperty::m_iGlowType);
        Color *glowColorOverride =
            (Color *)(glowProperty + schemas::CGlowProperty::m_glowColorOverride);

        *bGlowing = true;
        *iGlowType = 3;

        ImU32 baseColor = isTeammate
                              ? IM_COL32(64, 220, 120, 220)
                              : ToU32(hooks::espBoxColor);
        *glowColorOverride = Color((uint8_t)((baseColor >> IM_COL32_R_SHIFT) & 0xFF),
                                   (uint8_t)((baseColor >> IM_COL32_G_SHIFT) & 0xFF),
                                   (uint8_t)((baseColor >> IM_COL32_B_SHIFT) & 0xFF),
                                   (uint8_t)((baseColor >> IM_COL32_A_SHIFT) & 0xFF));
      } else {
        *bGlowing = false;
      }

      uintptr_t sceneNode =
          *(uintptr_t *)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
      if (!sceneNode)
        continue;

      Vector3 origin =
          *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
      Vector3 velocity =
          *(Vector3 *)(pawn + schemas::C_BaseEntity::m_vecAbsVelocity);
      EmitSoundCue(pawn, origin, velocity, isTeammate);

      uintptr_t boneArray =
          *(uintptr_t *)(sceneNode + schemas::CSkeletonInstance::m_modelState +
                         0x80);
      Vector3 headPos = boneArray ? GetBonePosition(boneArray, 6)
                                  : origin + Vector3(0.f, 0.f, 72.f);
      Vector3 neckPos = boneArray ? GetBonePosition(boneArray, 5)
                                  : origin + Vector3(0.f, 0.f, 60.f);
      if (IsZeroVec(headPos))
        headPos = origin + Vector3(0.f, 0.f, 72.f);
      if (IsZeroVec(neckPos))
        neckPos = origin + Vector3(0.f, 0.f, 60.f);

      Vector3 screenFoot, screenHead;
      if (!WorldToScreen(origin, viewMatrix, screenW, screenH, screenFoot) ||
          !WorldToScreen(headPos, viewMatrix, screenW, screenH, screenHead)) {
        continue;
      }

      float boxHeight = screenFoot.y - screenHead.y;
      if (boxHeight < 2.0f)
        continue;

      float boxWidth = boxHeight * 0.45f;
      float boxLeft = screenHead.x - boxWidth * 0.5f;
      float boxRight = boxLeft + boxWidth;
      float boxTop = screenHead.y;
      float boxBottom = screenFoot.y;

      ImU32 boxColor =
          hooks::espStyle == 1 ? ToU32(hooks::espCorneredColor)
                               : ToU32(hooks::espBoxColor);
      if (isTeammate)
        boxColor = IM_COL32(64, 220, 120, 220);
      else if (!isSpotted && hooks::espStyle != 2)
        boxColor &= IM_COL32(255, 255, 255, 180);

      if (hooks::espStyle == 0) {
        drawList->AddRect(ImVec2(boxLeft, boxTop), ImVec2(boxRight, boxBottom),
                          boxColor, 0.0f, 0, 1.8f);
      } else if (hooks::espStyle == 1) {
        DrawCornerBox(drawList, boxLeft, boxTop, boxWidth, boxBottom, boxColor);
      }

      if (hooks::espHealthBar) {
        float healthFrac = Clamp01((float)health / 100.0f);
        float barLeft = boxLeft - 6.0f;
        float fillTop = boxBottom - (boxBottom - boxTop) * healthFrac;
        drawList->AddRectFilled(ImVec2(barLeft, boxTop),
                                ImVec2(barLeft + 3.0f, boxBottom),
                                IM_COL32(0, 0, 0, 165));
        drawList->AddRectFilled(ImVec2(barLeft, fillTop),
                                ImVec2(barLeft + 3.0f, boxBottom),
                                LerpColor(hooks::espHealthStartColor,
                                          hooks::espHealthEndColor, healthFrac));
      }

      if (hooks::espArmorBar) {
        int armor = *(int *)(pawn + schemas::C_CSPlayerPawn::m_ArmorValue);
        float armorFrac = Clamp01((float)armor / 100.0f);
        float barRight = boxRight + 3.0f;
        float fillTop = boxBottom - (boxBottom - boxTop) * armorFrac;
        drawList->AddRectFilled(ImVec2(barRight, boxTop),
                                ImVec2(barRight + 3.0f, boxBottom),
                                IM_COL32(0, 0, 0, 165));
        drawList->AddRectFilled(ImVec2(barRight, fillTop),
                                ImVec2(barRight + 3.0f, boxBottom),
                                ToU32(hooks::espArmorColor));
      }

      float textY = boxTop - 16.0f;
      if (hooks::espNameEnabled) {
        uintptr_t namePtr =
            *(uintptr_t *)(controller + schemas::CCSPlayerController::m_sSanitizedPlayerName);
        if (namePtr) {
          const char *name = (const char *)namePtr;
          uint64_t steamID =
              *(uint64_t *)(controller + schemas::CBasePlayerController::m_steamID);

          char displayName[128];
          if (steamID == 0)
            snprintf(displayName, sizeof(displayName), "[BOT] %s", name);
          else
            snprintf(displayName, sizeof(displayName), "%s", name);

          ImVec2 textSize = ImGui::CalcTextSize(displayName);
          drawList->AddText(ImVec2(screenHead.x - textSize.x * 0.5f, textY),
                            ToU32(hooks::espNameColor), displayName);
        }
      }

      if (hooks::espHeadCircle) {
        Vector3 screenNeck;
        if (WorldToScreen(neckPos, viewMatrix, screenW, screenH, screenNeck)) {
          float radius = (std::max)(4.0f, std::fabs(screenNeck.y - screenHead.y) * 0.75f);
          drawList->AddCircle(ImVec2(screenHead.x, screenHead.y), radius,
                              ToU32(hooks::espHeadCircleColor), 24, 1.5f);
        }
      }

      if (hooks::espSkeletonEnabled && boneArray) {
        DrawSkeleton(drawList, boneArray, viewMatrix, screenW, screenH,
                     ToU32(hooks::espSkeletonColor));
      }

      if (hooks::espWeaponEnabled || hooks::espWeaponIcon) {
        uint16_t activeWeapon = GetActiveWeaponDefIndex(pawn, entityList);
        if (activeWeapon) {
          const char *label =
              WeaponLabel(activeWeapon, hooks::espWeaponIcon && !hooks::espWeaponEnabled);
          char weaponText[64];
          if (hooks::espWeaponEnabled && hooks::espWeaponIcon) {
            snprintf(weaponText, sizeof(weaponText), "%s | %s",
                     WeaponLabel(activeWeapon, true), WeaponLabel(activeWeapon, false));
            label = weaponText;
          }

          ImVec2 textSize = ImGui::CalcTextSize(label);
          drawList->AddText(ImVec2(screenHead.x - textSize.x * 0.5f, boxBottom + 2.0f),
                            ToU32(hooks::espWeaponColor), label);
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      continue;
    }
  }

  DrawSoundCues(drawList, viewMatrix, screenW, screenH);
}
