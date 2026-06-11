#include "pch.h"

namespace {

static float ReadTickBaseFallback(uintptr_t clientBase) {
  uintptr_t localController =
      *(uintptr_t *)(clientBase + offsets::dwLocalPlayerController);
  if (!localController)
    return 0.0f;

  uint32_t tickBase =
      *(uint32_t *)(localController + schemas::CBasePlayerController::m_nTickBase);
  if (tickBase == 0 || tickBase > 1000000)
    return 0.0f;

  return (float)tickBase * (1.0f / 64.0f);
}

} // namespace

static float ReadCurTimeImpl(uintptr_t clientBase) {
  uintptr_t globalVarsCandidates[2] = {
      *(uintptr_t *)(clientBase + offsets::dwGlobalVars),
      clientBase + offsets::dwGlobalVars,
  };
  const std::ptrdiff_t curTimeOffsets[] = {0x2C, 0x30, 0x34, 0x38};

  for (uintptr_t base : globalVarsCandidates) {
    if (!base || base < 0x10000)
      continue;
    for (std::ptrdiff_t off : curTimeOffsets) {
      float curTime = *(float *)(base + off);
      if (curTime > 0.0f && curTime < 100000.0f)
        return curTime;
    }
  }
  return 0.0f;
}

float bombtimer::ReadCurTime() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return 0.0f;
  float curTime = ReadCurTimeImpl(clientBase);
  if (curTime > 0.0f)
    return curTime;
  return ReadTickBaseFallback(clientBase);
}

uintptr_t bombtimer::FindPlantedC4Entity() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return 0;

  __try {
    uintptr_t c4Node = *(uintptr_t *)(clientBase + offsets::dwPlantedC4);
    if (!c4Node || c4Node < 0x10000)
      return 0;

    // dwPlantedC4 points to a pointer to the entity (or a list)
    uintptr_t entity = *(uintptr_t *)c4Node;
    if (!entity || entity < 0x10000)
      return 0;

    // Validate the entity by checking m_bBombTicking
    bool ticking = *(bool *)(entity + schemas::C_PlantedC4::m_bBombTicking);
    if (!ticking)
      return 0;

    // Extra validation: blow time must be reasonable
    float blowTime = *(float *)(entity + schemas::C_PlantedC4::m_flC4Blow);
    if (blowTime <= 0.0f || blowTime > 1000000.0f)
      return 0;

    // Check the scene node exists (proves it's a real entity)
    uintptr_t sceneNode =
        *(uintptr_t *)(entity + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode || sceneNode < 0x10000)
      return 0;

    return entity;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

void bombtimer::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t c4Entity = FindPlantedC4Entity();
  if (!c4Entity)
    return;

  // Check if bomb is actually ticking
  bool ticking = *(bool *)(c4Entity + schemas::C_PlantedC4::m_bBombTicking);
  if (!ticking)
    return;

  // Blow time (the exact game time the bomb will explode)
  float blowTime = *(float *)(c4Entity + schemas::C_PlantedC4::m_flC4Blow);
  if (blowTime <= 0.f)
    return;

  float curTime = ReadCurTime();
  if (curTime <= 0.0f)
    return;

  float remaining = blowTime - curTime;

  // Validate remaining time (bomb timer is typically 40s)
  if (remaining <= 0.f || remaining > 45.f)
    return;

  bool beingDefused =
      *(bool *)(c4Entity + schemas::C_PlantedC4::m_bBeingDefused);
  float defuseEndTime =
      *(float *)(c4Entity + schemas::C_PlantedC4::m_flDefuseCountDown);
  float defuseRemaining = beingDefused ? (defuseEndTime - curTime) : 0.0f;
  int bombSite = *(int *)(c4Entity + schemas::C_PlantedC4::m_nBombSite);

  // Draw
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  float centerX = displaySize.x * 0.5f;
  float yPos = 80.f; // Moved down slightly to not overlap with text

  ImU32 timerColor = IM_COL32(255, 255, 255, 255);
  if (remaining < 5.f)
    timerColor = IM_COL32(255, 0, 0, 255); // Red when very close
  else if (remaining < 10.f)
    timerColor = IM_COL32(255, 165, 0, 255); // Orange

  float barWidth = 260.f, barHeight = 20.f;
  float barLeft = centerX - barWidth * 0.5f;
  float barFrac = remaining / 40.0f;
  if (barFrac > 1.f)
    barFrac = 1.f;
  if (barFrac < 0.f)
    barFrac = 0.f;

  // Background
  drawList->AddRectFilled(ImVec2(barLeft, yPos),
                          ImVec2(barLeft + barWidth, yPos + barHeight),
                          IM_COL32(0, 0, 0, 160), 4.f);

  // Fill
  drawList->AddRectFilled(
      ImVec2(barLeft + 2, yPos + 2),
      ImVec2(barLeft + 2 + (barWidth - 4) * barFrac, yPos + barHeight - 2),
      timerColor, 3.f);

  // Text overlay
  char text[96];
  snprintf(text, sizeof(text), "BOMB %c: %.1fs",
           bombSite == 1 ? 'B' : 'A', remaining);
  ImVec2 textSize = ImGui::CalcTextSize(text);
  drawList->AddText(ImVec2(centerX - textSize.x * 0.5f, yPos + 2.f),
                    IM_COL32(255, 255, 255, 255), text);

  if (beingDefused && defuseRemaining > 0.0f) {
    float defuseFrac = defuseRemaining / 10.0f;
    if (defuseFrac < 0.0f)
      defuseFrac = 0.0f;
    if (defuseFrac > 1.0f)
      defuseFrac = 1.0f;

    float defuseY = yPos + barHeight + 8.0f;
    drawList->AddRectFilled(ImVec2(barLeft, defuseY),
                            ImVec2(barLeft + barWidth, defuseY + barHeight),
                            IM_COL32(0, 0, 0, 160), 4.f);

    ImU32 defuseColor =
        defuseRemaining <= remaining ? IM_COL32(0, 220, 120, 255)
                                     : IM_COL32(255, 80, 80, 255);
    drawList->AddRectFilled(
        ImVec2(barLeft + 2.0f, defuseY + 2.0f),
        ImVec2(barLeft + 2.0f + (barWidth - 4.0f) * defuseFrac,
               defuseY + barHeight - 2.0f),
        defuseColor, 3.0f);

    char defuseText[96];
    snprintf(defuseText, sizeof(defuseText), "DEFUSE: %.1fs %s", defuseRemaining,
             defuseRemaining <= remaining ? "(safe)" : "(too late)");
    ImVec2 defuseSize = ImGui::CalcTextSize(defuseText);
    drawList->AddText(
        ImVec2(centerX - defuseSize.x * 0.5f, defuseY + 2.0f),
        IM_COL32(255, 255, 255, 255), defuseText);
  }

  if (hooks::bombDefuseCircle) {
    uintptr_t sceneNode =
        *(uintptr_t *)(c4Entity + schemas::C_BaseEntity::m_pGameSceneNode);
    if (sceneNode) {
      Vector3 origin =
          *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
      view_matrix_t &viewMatrix =
          *(view_matrix_t *)(clientBase + offsets::dwViewMatrix);
      Vector3 screen;
      if (WorldToScreen(origin, viewMatrix, displaySize.x, displaySize.y, screen)) {
        float explodeArc = remaining / 40.0f;
        if (explodeArc < 0.0f)
          explodeArc = 0.0f;
        if (explodeArc > 1.0f)
          explodeArc = 1.0f;

        drawList->AddCircle(ImVec2(screen.x, screen.y), 24.0f,
                            IM_COL32(255, 120, 40, 160), 32, 2.0f);
        drawList->PathArcTo(ImVec2(screen.x, screen.y), 20.0f, -3.14159f * 0.5f,
                            -3.14159f * 0.5f + (6.28318f * explodeArc), 32);
        drawList->PathStroke(timerColor, 0, 3.0f);

        if (beingDefused && defuseRemaining > 0.0f) {
          float defuseArc = defuseRemaining / 10.0f;
          if (defuseArc < 0.0f)
            defuseArc = 0.0f;
          if (defuseArc > 1.0f)
            defuseArc = 1.0f;

          drawList->PathArcTo(ImVec2(screen.x, screen.y), 28.0f, -3.14159f * 0.5f,
                              -3.14159f * 0.5f + (6.28318f * defuseArc), 32);
          drawList->PathStroke(
              defuseRemaining <= remaining ? IM_COL32(0, 220, 120, 220)
                                           : IM_COL32(255, 80, 80, 220),
              0, 3.0f);
        }
      }
    }
  }
}
