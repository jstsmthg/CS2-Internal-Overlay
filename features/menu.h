#pragma once

namespace menu {

// Navigation tabs
enum Tab {
  Tab_Aimbot = 0,
  Tab_Visuals,
  Tab_WorldESP,
  Tab_Misc,
  Tab_GrenadeHelper,
  Tab_Transcript,
  Tab_Config,
  Tab_COUNT
};

inline int currentTab = Tab_Aimbot;

// Apply the custom dark theme (call once after ImGui init)
void ApplyStyle();

// Render the full menu (call every frame inside NewFrame/Render)
void Render();

} // namespace menu
