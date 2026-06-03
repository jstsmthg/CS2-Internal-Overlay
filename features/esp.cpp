#include "pch.h"

void esp::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  int localTeam = *(int *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);

  uintptr_t localSceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  Vector3 localOrigin = {};
  if (localSceneNode)
    localOrigin =
        *(Vector3 *)(localSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);

  view_matrix_t &viewMatrix =
      *(view_matrix_t *)(clientBase + offsets::dwViewMatrix);

  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  float screenW = displaySize.x;
  float screenH = displaySize.y;

  ImDrawList *drawList = ImGui::GetBackgroundDrawList();

  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  uint32_t spectatedPawnHandle = 0;
  if (localPawn) {
    uintptr_t obsServices = *(
        uintptr_t *)(localPawn + schemas::C_BasePlayerPawn::m_pObserverServices);
    if (obsServices) {
      spectatedPawnHandle = *(
          uint32_t *)(obsServices + schemas::CPlayer_ObserverServices::m_hObserverTarget);
    }
  }
  // Track seen pawn addresses to prevent double-drawing
  uintptr_t seenPawns[64] = {};
  int seenCount = 0;

  for (int i = 1; i <= 64; i++) {
    __try {
      uintptr_t listEntry =
          *(uintptr_t *)(entityList + 0x10 + 8 * ((i & 0x7FFF) >> 9));
      if (!listEntry)
        continue;

      uintptr_t controller = *(uintptr_t *)(listEntry + 0x70 * (i & 0x1FF));
      if (!controller)
        continue;

      uint32_t pawnHandle = *(
          uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
      if (!pawnHandle || pawnHandle == 0xFFFFFFFF)
        continue;

      uintptr_t pawnEntry =
          *(uintptr_t *)(entityList + 0x10 + 8 * ((pawnHandle & 0x7FFF) >> 9));
      if (!pawnEntry)
        continue;

      uintptr_t pawn =
          *(uintptr_t *)(pawnEntry + 0x70 * ((pawnHandle & 0x7FFF) & 0x1FF));
      if (!pawn || pawn == localPawn)
        continue;

      // Skip duplicate pawn entries (prevents double overlay)
      bool isDuplicate = false;
      for (int s = 0; s < seenCount; s++) {
        if (seenPawns[s] == pawn) { isDuplicate = true; break; }
      }
      if (isDuplicate) continue;
      if (seenCount < 64) seenPawns[seenCount++] = pawn;

      int health = *(int *)(pawn + schemas::C_BaseEntity::m_iHealth);
      int team = *(uint8_t *)(pawn + schemas::C_BaseEntity::m_iTeamNum);

      if (health <= 0)
        continue;

      bool isTeammate = (team == localTeam) && !hooks::ignoreTeam;

      // Radar hack
      if (hooks::radarEnabled && !isTeammate) {
        *(bool *)(pawn + schemas::C_CSPlayerPawn::m_entitySpottedState +
                  schemas::EntitySpottedState_t::m_bSpotted) = true;
      }

      bool isSpotted =
          *(bool *)(pawn + schemas::C_CSPlayerPawn::m_entitySpottedState +
                    schemas::EntitySpottedState_t::m_bSpotted);

      // --- Glow ESP ---
      uintptr_t glowProperty = pawn + schemas::C_BaseModelEntity::m_Glow;
      bool *bGlowing = (bool *)(glowProperty + schemas::CGlowProperty::m_bGlowing);

      if (hooks::espStyle == 1) {
        int *iGlowType = (int *)(glowProperty + schemas::CGlowProperty::m_iGlowType);
        Color *glowColorOverride =
            (Color *)(glowProperty + schemas::CGlowProperty::m_glowColorOverride);

        *bGlowing = true;
        *iGlowType = 3;

        if (isTeammate) {
          *glowColorOverride = Color(50, 205, 50, 255);
        } else if (isSpotted) {
          *glowColorOverride = (team == 3) ? Color(100, 180, 255, 255)
                                           : Color(255, 80, 80, 255);
        } else {
          *glowColorOverride = (team == 3) ? Color(100, 149, 237, 120)
                                           : Color(255, 99, 71, 120);
        }
      } else {
        *bGlowing = false;
      }
      // ----------------

      uintptr_t gameSceneNode =
          *(uintptr_t *)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
      if (!gameSceneNode)
        continue;

      Vector3 origin =
          *(Vector3 *)(gameSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
      Vector3 head = origin + Vector3(0.f, 0.f, 75.f);

      Vector3 screenFoot, screenHead;
      if (!WorldToScreen(origin, viewMatrix, screenW, screenH, screenFoot))
        continue;
      if (!WorldToScreen(head, viewMatrix, screenW, screenH, screenHead))
        continue;

      float boxHeight = screenFoot.y - screenHead.y;
      if (boxHeight < 2.f)
        continue;
      float boxWidth = boxHeight * 0.45f;
      float boxLeft = screenHead.x - boxWidth * 0.5f;

      if (hooks::espStyle == 0) {
        ImU32 boxColor;
        if (isTeammate) {
          boxColor = IM_COL32(50, 205, 50, 255);
        } else if (isSpotted) {
          boxColor = (team == 3) ? IM_COL32(100, 180, 255, 255)
                                 : IM_COL32(255, 80, 80, 255);
        } else {
          boxColor = (team == 3) ? IM_COL32(100, 149, 237, 120)
                                 : IM_COL32(255, 99, 71, 120);
        }

        drawList->AddRect(ImVec2(boxLeft, screenHead.y),
                          ImVec2(boxLeft + boxWidth, screenFoot.y), boxColor, 0.f,
                          0, 2.0f);
      }

      // Health bar
      float healthFrac = (float)health / 100.0f;
      if (healthFrac > 1.f)
        healthFrac = 1.f;
      float barW = 3.f, barL = boxLeft - barW - 2.f;
      float barTop = screenHead.y, barBot = screenFoot.y;
      float barFill = barBot - (barBot - barTop) * healthFrac;
      drawList->AddRectFilled(ImVec2(barL, barTop), ImVec2(barL + barW, barBot),
                              IM_COL32(0, 0, 0, 180));
      ImU32 hpCol = (healthFrac > 0.5f)
                        ? IM_COL32(0, (int)(255 * healthFrac), 0, 255)
                        : IM_COL32(255, (int)(255 * healthFrac * 2), 0, 255);
      drawList->AddRectFilled(ImVec2(barL, barFill),
                              ImVec2(barL + barW, barBot), hpCol);

      char hpText[16];
      snprintf(hpText, sizeof(hpText), "%d HP", health);
      drawList->AddText(ImVec2(boxLeft + boxWidth + 4.f, screenHead.y),
                        IM_COL32(255, 255, 255, 220), hpText);

      float dist = (origin - localOrigin).Length() * 0.0254f;
      char distText[16];
      snprintf(distText, sizeof(distText), "%.0fm", dist);
      drawList->AddText(ImVec2(boxLeft + boxWidth + 4.f, screenHead.y + 14.f),
                        IM_COL32(200, 200, 200, 200), distText);

      uintptr_t namePtr =
          *(uintptr_t *)(controller +
                         schemas::CCSPlayerController::m_sSanitizedPlayerName);
      if (namePtr) {
        const char *name = (const char *)namePtr;
        uint64_t steamID = *(uint64_t*)(controller + schemas::CBasePlayerController::m_steamID);
        
        char displayName[128];
        if (steamID == 0) {
            snprintf(displayName, sizeof(displayName), "[BOT] %s", name);
        } else {
            snprintf(displayName, sizeof(displayName), "%s", name);
        }

        ImU32 nameColor = isTeammate ? IM_COL32(50, 205, 50, 240)
                                     : IM_COL32(255, 99, 71, 240);
        
        ImVec2 nameSize = ImGui::CalcTextSize(displayName);
        drawList->AddText(
            ImVec2(screenHead.x - nameSize.x * 0.5f, screenHead.y - 14.f),
            nameColor, displayName);
      }

      if (!isTeammate && isSpotted) {
        drawList->AddCircleFilled(
            ImVec2(boxLeft + boxWidth + 4.f, screenHead.y + 30.f), 4.f,
            IM_COL32(255, 255, 0, 255));
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      continue;
    }
  }
}
