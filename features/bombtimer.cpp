#include "pch.h"

void bombtimer::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  // The C4 planted entity pointer
  uintptr_t c4Entity = *(uintptr_t *)(clientBase + offsets::dwPlantedC4);
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

  // Current game time from GlobalVars
  // dwGlobalVars is a pointer TO a pointer in CS2
  uintptr_t globalVarsPtr = *(uintptr_t *)(clientBase + offsets::dwGlobalVars);
  if (!globalVarsPtr)
    return;

  // Read curtime (offset 0x34 is standard for recent CS2 GlobalVars)
  float curTime = *(float *)(globalVarsPtr + 0x34);

  // If curTime is somehow 0, try offset 0x2C
  if (curTime <= 0.f) {
    curTime = *(float *)(globalVarsPtr + 0x2C);
  }

  float remaining = blowTime - curTime;

  // Validate remaining time (bomb timer is typically 40s)
  if (remaining <= 0.f || remaining > 45.f)
    return;

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

  float barWidth = 200.f, barHeight = 20.f;
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
  char text[64];
  snprintf(text, sizeof(text), "BOMB: %.1fs", remaining);
  ImVec2 textSize = ImGui::CalcTextSize(text);
  drawList->AddText(ImVec2(centerX - textSize.x * 0.5f, yPos + 2.f),
                    IM_COL32(255, 255, 255, 255), text);
}
