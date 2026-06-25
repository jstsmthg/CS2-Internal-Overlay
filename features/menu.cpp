#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#include "imgui_internal.h"

// ============================================================================
// Color palette (anyx.gg inspired dark theme)
// ============================================================================
static const ImU32 kAccentU32       = IM_COL32(0, 212, 255, 255);
static const ImU32 kAccentDimU32    = IM_COL32(0, 212, 255, 100);
static const ImU32 kToggleOffU32    = IM_COL32(60, 65, 75, 255);
static const ImU32 kTextU32         = IM_COL32(210, 215, 220, 255);
static const ImU32 kTextDimU32      = IM_COL32(130, 140, 150, 255);
static const ImU32 kSidebarBgU32    = IM_COL32(17, 21, 28, 255);
static const ImU32 kContentBgU32    = IM_COL32(13, 17, 23, 255);
static const ImU32 kSectionLineU32  = IM_COL32(0, 212, 255, 80);
static const ImU32 kSectionBorderU32= IM_COL32(35, 45, 55, 255);

static const ImVec4 kAccent      = ImVec4(0.00f, 0.83f, 1.00f, 1.00f);
static const ImVec4 kAccentDim   = ImVec4(0.00f, 0.83f, 1.00f, 0.40f);
static const ImVec4 kTextCol     = ImVec4(0.82f, 0.84f, 0.86f, 1.00f);
static const ImVec4 kTextDim     = ImVec4(0.50f, 0.55f, 0.59f, 1.00f);
static const ImVec4 kWarning     = ImVec4(1.00f, 0.75f, 0.00f, 1.00f);

// ============================================================================
// Dimensions
// ============================================================================
static constexpr float kMenuW      = 860.0f;
static constexpr float kMenuH      = 560.0f;
static constexpr float kSidebarW   = 175.0f;
static constexpr float kPad        = 10.0f;

// Forward declare the VK key name helper from present.cpp
static const char *GetVKName(int vk) {
  switch (vk) {
  case VK_LBUTTON:  return "Mouse1";
  case VK_RBUTTON:  return "Mouse2";
  case VK_MBUTTON:  return "Mouse3";
  case VK_XBUTTON1: return "Mouse4";
  case VK_XBUTTON2: return "Mouse5";
  case VK_LSHIFT:   return "LShift";
  case VK_RSHIFT:   return "RShift";
  case VK_LCONTROL: return "LCtrl";
  case VK_RCONTROL: return "RCtrl";
  case VK_LMENU:    return "LAlt";
  case VK_RMENU:    return "RAlt";
  case VK_CAPITAL:  return "CapsLock";
  case VK_TAB:      return "Tab";
  case VK_SPACE:    return "Space";
  case VK_ESCAPE:   return "Escape";
  case VK_INSERT:   return "Insert";
  case VK_DELETE:    return "Delete";
  case VK_END:      return "End";
  default: {
    if (vk >= 0x30 && vk <= 0x39) { static char b[2]; b[0]=(char)vk; b[1]=0; return b; }
    if (vk >= 0x41 && vk <= 0x5A) { static char b[2]; b[0]=(char)vk; b[1]=0; return b; }
    if (vk >= VK_F1 && vk <= VK_F12) { static char b[4]; snprintf(b,4,"F%d",vk-VK_F1+1); return b; }
    static char b[8]; snprintf(b,8,"0x%02X",vk); return b;
  }
  }
}

// ============================================================================
// Custom Widgets
// ============================================================================

// --- Toggle Switch (pill-shaped on/off) ---
// reserveRight: pixels to leave free on the right for color pickers etc.
static bool Toggle(const char *label, bool *v, float reserveRight = 0.0f) {
  ImGuiWindow *window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  const ImGuiID id = window->GetID(label);
  const ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);
  const float h = ImGui::GetFrameHeight() * 0.70f;
  const float w = h * 1.80f;
  const float availW = ImGui::GetContentRegionAvail().x;
  const float usedW = availW - reserveRight;
  const ImVec2 pos = window->DC.CursorPos;

  // Bounding box only covers usedW, leaving reserveRight for other items
  const ImRect totalBB(pos, ImVec2(pos.x + usedW, pos.y + h + 4.0f));
  // But item size covers the full available width so next item goes below
  ImGui::ItemSize(ImRect(pos, ImVec2(pos.x + availW, pos.y + h + 4.0f)), 0.0f);
  if (!ImGui::ItemAdd(totalBB, id))
    return false;

  bool hovered, held;
  bool pressed = ImGui::ButtonBehavior(totalBB, id, &hovered, &held);
  if (pressed) {
    *v = !(*v);
    ImGui::MarkItemEdited(id);
  }

  // Toggle track (right-aligned within usedW)
  float tx = pos.x + usedW - w;
  float ty = pos.y + 2.0f;
  float r = h * 0.5f;
  ImU32 bgCol = *v ? kAccentU32 : kToggleOffU32;
  window->DrawList->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + w, ty + h),
                                  bgCol, r);

  // Circle knob
  float cr = h * 0.38f;
  float cx = *v ? (tx + w - cr - 3.0f) : (tx + cr + 3.0f);
  float cy = ty + h * 0.5f;
  window->DrawList->AddCircleFilled(ImVec2(cx, cy), cr,
                                    IM_COL32(255, 255, 255, 240));

  // Label (left-aligned)
  ImGui::RenderText(ImVec2(pos.x, pos.y + (h + 4.0f - labelSize.y) * 0.5f),
                    label);

  return pressed;
}

// --- Section Header with cyan underline ---
static void SectionHeader(const char *title) {
  ImGui::Spacing();
  ImGui::Spacing();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  float w = ImGui::GetContentRegionAvail().x;

  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                        (w - ImGui::CalcTextSize(title).x) * 0.5f);
  ImGui::TextColored(kTextCol, "%s", title);

  // Thin accent line
  ImVec2 lineY = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(pos.x, lineY.y), ImVec2(pos.x + w, lineY.y), kSectionLineU32,
      1.0f);
  ImGui::Spacing();
  ImGui::Spacing();
}

// --- Key Bind Button ---
static bool KeyBind(const char *label, int *key, bool *waiting) {
  char buf[64];
  if (*waiting) {
    snprintf(buf, sizeof(buf), "[...]##%s", label);
  } else if (*key == 0) {
    snprintf(buf, sizeof(buf), "[None]##%s", label);
  } else {
    snprintf(buf, sizeof(buf), "[%s]##%s", GetVKName(*key), label);
  }

  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.15f, 0.18f, 0.22f, 1.0f));
  bool clicked = ImGui::SmallButton(buf);
  ImGui::PopStyleColor(2);

  if (clicked && !*waiting) {
    *waiting = true;
    hooks::pendingKeyBind = key;
    hooks::pendingKeyBindFlag = waiting;
  }
  return clicked;
}

// --- Inline Color Edit (small square button that opens popup) ---
static void ColorEdit(const char *label, float *col) {
  ImGui::PushID(label);
  ImVec4 c(col[0], col[1], col[2], col[3]);
  if (ImGui::ColorButton("##btn", c,
                         ImGuiColorEditFlags_NoTooltip |
                             ImGuiColorEditFlags_AlphaPreview |
                             ImGuiColorEditFlags_NoBorder,
                         ImVec2(16, 16))) {
    ImGui::OpenPopup("##picker");
  }
  if (ImGui::BeginPopup("##picker")) {
    ImGui::ColorPicker4("##cp", col,
                        ImGuiColorEditFlags_AlphaBar |
                            ImGuiColorEditFlags_NoSidePreview);
    ImGui::EndPopup();
  }
  ImGui::PopID();
}

// --- Toggle with color picker on the right ---
// Draws: [Label] .............. [ColorBtn] [ToggleSwitch]
static void ToggleColor(const char *label, bool *enabled, float *col) {
  const float colorSpace = col ? 26.0f : 0.0f;
  Toggle(label, enabled, colorSpace);
  if (col) {
    // Draw color button in the reserved space to the right of the toggle
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
    ColorEdit(label, col);
  }
}

// ============================================================================
// Style Setup
// ============================================================================
void menu::ApplyStyle() {
  ImGuiStyle &s = ImGui::GetStyle();

  // Geometry
  s.WindowRounding = 8.0f;
  s.ChildRounding = 4.0f;
  s.FrameRounding = 4.0f;
  s.GrabRounding = 4.0f;
  s.PopupRounding = 4.0f;
  s.ScrollbarRounding = 4.0f;
  s.WindowBorderSize = 0.0f;
  s.ChildBorderSize = 1.0f;
  s.FrameBorderSize = 0.0f;
  s.WindowPadding = ImVec2(0, 0);
  s.FramePadding = ImVec2(6, 4);
  s.ItemSpacing = ImVec2(8, 5);
  s.ScrollbarSize = 10.0f;
  s.GrabMinSize = 8.0f;

  // Colors
  ImVec4 *c = s.Colors;
  c[ImGuiCol_WindowBg]         = ImVec4(0.051f, 0.067f, 0.090f, 0.97f);
  c[ImGuiCol_ChildBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  c[ImGuiCol_PopupBg]          = ImVec4(0.07f, 0.09f, 0.12f, 0.95f);
  c[ImGuiCol_Border]           = ImVec4(0.14f, 0.18f, 0.22f, 0.60f);
  c[ImGuiCol_Text]             = ImVec4(0.82f, 0.84f, 0.86f, 1.00f);
  c[ImGuiCol_TextDisabled]     = ImVec4(0.45f, 0.50f, 0.55f, 1.00f);
  c[ImGuiCol_FrameBg]          = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
  c[ImGuiCol_FrameBgHovered]   = ImVec4(0.14f, 0.17f, 0.21f, 1.00f);
  c[ImGuiCol_FrameBgActive]    = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
  c[ImGuiCol_TitleBg]          = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
  c[ImGuiCol_TitleBgActive]    = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
  c[ImGuiCol_ScrollbarBg]      = ImVec4(0.05f, 0.07f, 0.09f, 0.50f);
  c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
  c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.32f, 0.38f, 1.00f);
  c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.00f, 0.83f, 1.00f, 1.00f);
  c[ImGuiCol_CheckMark]        = ImVec4(0.00f, 0.83f, 1.00f, 1.00f);
  c[ImGuiCol_SliderGrab]       = ImVec4(0.00f, 0.83f, 1.00f, 1.00f);
  c[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.93f, 1.00f, 1.00f);
  c[ImGuiCol_Button]           = ImVec4(0.10f, 0.13f, 0.17f, 1.00f);
  c[ImGuiCol_ButtonHovered]    = ImVec4(0.15f, 0.19f, 0.24f, 1.00f);
  c[ImGuiCol_ButtonActive]     = ImVec4(0.00f, 0.65f, 0.85f, 1.00f);
  c[ImGuiCol_Header]           = ImVec4(0.10f, 0.13f, 0.17f, 1.00f);
  c[ImGuiCol_HeaderHovered]    = ImVec4(0.15f, 0.19f, 0.24f, 1.00f);
  c[ImGuiCol_HeaderActive]     = ImVec4(0.00f, 0.65f, 0.85f, 1.00f);
  c[ImGuiCol_Separator]        = ImVec4(0.14f, 0.18f, 0.22f, 0.60f);
  c[ImGuiCol_Tab]              = ImVec4(0.10f, 0.13f, 0.17f, 1.00f);
  c[ImGuiCol_TabHovered]       = ImVec4(0.00f, 0.65f, 0.85f, 0.60f);
}

// ============================================================================
// Sidebar
// ============================================================================
static const char *kTabNames[] = {"Aimbot", "Visuals", "World ESP", "Misc",
                                  "Grenades", "Transcript", "Config"};
static const char *kTabIcons[] = {"+", "@", "#", "*", "%", "~", "="};

static void RenderSidebar() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ImVec4(0.067f, 0.082f, 0.110f, 1.0f));
  ImGui::BeginChild("##sidebar", ImVec2(kSidebarW, -1), false,
                    ImGuiWindowFlags_NoScrollbar);

  // --- Logo ---
  ImGui::Spacing();
  ImGui::Spacing();
  ImGui::SetCursorPosX((kSidebarW - ImGui::CalcTextSize("CS2 OVERLAY").x) *
                        0.5f);
  ImGui::TextColored(kAccent, "CS2 OVERLAY");
  ImGui::Spacing();

  // Accent separator
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(p.x + 15, p.y), ImVec2(p.x + kSidebarW - 15, p.y),
      kAccentDimU32, 1.0f);
  ImGui::Spacing();
  ImGui::Spacing();

  // --- Tab buttons ---
  for (int i = 0; i < menu::Tab_COUNT; i++) {
    bool selected = (menu::currentTab == i);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Active indicator bar (left edge)
    if (selected) {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(cursorPos.x, cursorPos.y),
          ImVec2(cursorPos.x + 3, cursorPos.y + 28), kAccentU32, 2.0f);
    }

    // Button
    ImGui::PushStyleColor(
        ImGuiCol_Button,
        selected ? ImVec4(0.00f, 0.40f, 0.55f, 0.20f)
                 : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.00f, 0.40f, 0.55f, 0.15f));
    ImGui::PushStyleColor(
        ImGuiCol_Text, selected ? kAccent : kTextDim);

    ImGui::SetCursorPosX(12.0f);
    char btnId[64];
    snprintf(btnId, sizeof(btnId), "  %s  %s##tab%d", kTabIcons[i],
             kTabNames[i], i);
    if (ImGui::Button(btnId, ImVec2(kSidebarW - 16, 28))) {
      menu::currentTab = i;
    }

    ImGui::PopStyleColor(3);
  }

  // --- Bottom info ---
  ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 40);
  ImGui::SetCursorPosX(10);
  ImGui::TextColored(kTextDim, "Press [END] to eject");

  ImGui::EndChild();
  ImGui::PopStyleColor(); // ChildBg
}

// ============================================================================
// Tab: Aimbot
// ============================================================================
static void RenderAimbotTab() {
  float colW = (ImGui::GetContentRegionAvail().x - kPad) * 0.5f;

  // ---- Left column ----
  ImGui::BeginChild("##aim_left", ImVec2(colW, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Aimbot");
  Toggle("Enable", &hooks::aimbotEnabled);

  if (hooks::aimbotEnabled) {
    ImGui::Text("Aim Key");
    KeyBind("aimKey", &hooks::aimbotKey, &hooks::waitingForAimbotKey);

    ImGui::Combo("Weapon Category", &hooks::aimbotWeaponCat,
                 "All\0Rifles\0SMG\0Pistols\0Snipers\0Shotguns\0");

    ImGui::Combo("Hitbox", &hooks::aimbotHitbox,
                 "Head\0Neck\0Chest\0Pelvis\0Legs\0");
    Toggle("Closest Hitbox", &hooks::aimbotClosestHitbox);
    Toggle("Visibility Check", &hooks::aimbotVisCheck);

    ImGui::SliderFloat("FOV", &hooks::aimbotFov, 1.0f, 90.0f, "%.1f");
    ImGui::SliderFloat("Smoothing", &hooks::aimbotSmoothing, 1.0f, 20.0f,
                        "%.1f");
    Toggle("Linear Smoothing", &hooks::aimbotLinearSmooth);


  }

  ImGui::Spacing();
  SectionHeader("RCS");
  Toggle("Recoil Control", &hooks::rcsEnabled);
  if (hooks::rcsEnabled) {
    ImGui::SliderFloat("Recoil X", &hooks::rcsX, 0.0f, 100.0f, "%.0f%%");
    ImGui::SliderFloat("Recoil Y", &hooks::rcsY, 0.0f, 100.0f, "%.0f%%");
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::SameLine(0, kPad);

  // ---- Right column ----
  ImGui::BeginChild("##aim_right", ImVec2(0, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Triggerbot");
  Toggle("Enable", &hooks::triggerbotEnabled);
  if (hooks::triggerbotEnabled) {
    ImGui::SliderInt("Shoot Delay (ms)", &hooks::triggerbotDelay, 0, 200);

    ImGui::Spacing();
    Toggle("Trigger Aim", &hooks::triggerbotAim);
    if (hooks::triggerbotAim) {
      ImGui::Text("Aim Key");
      KeyBind("trigAimKey", &hooks::triggerbotAimKey,
              &hooks::waitingForTriggerAimKey);
      Toggle("Use Recoil", &hooks::triggerbotAimUseRecoil);
      ImGui::SliderFloat("FOV##trig", &hooks::triggerbotAimFov, 1.0f, 90.0f,
                          "%.1f");
      ImGui::SliderFloat("Smooth##trig", &hooks::triggerbotAimSmooth, 1.0f,
                          20.0f, "%.1f");
      ImGui::Combo("Hitbox##trig", &hooks::triggerbotAimHitbox,
                   "Head\0Neck\0Chest\0Pelvis\0Legs\0");
    }
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();
}

// ============================================================================
// Tab: Visuals
// ============================================================================
static void RenderVisualsTab() {
  float colW = (ImGui::GetContentRegionAvail().x - kPad) * 0.5f;

  // ---- Left column ----
  ImGui::BeginChild("##vis_left", ImVec2(colW, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Player ESP");
  Toggle("Enable ESP", &hooks::espEnabled);

  if (hooks::espEnabled) {
    ImGui::Text("Toggle Key");
    KeyBind("espKey", &hooks::espToggleKey, &hooks::waitingForEspKey);

    ImGui::Combo("Box Style", &hooks::espStyle,
                 "2D Box\0Cornered Box\0Glow (Silhouette)\0");

    // Box/Cornered color
    if (hooks::espStyle == 0) {
      ImGui::Text("Box Color");
      ImGui::SameLine();
      ColorEdit("boxCol", hooks::espBoxColor);
    } else if (hooks::espStyle == 1) {
      ImGui::Text("Corner Color");
      ImGui::SameLine();
      ColorEdit("cornerCol", hooks::espCorneredColor);
    }

    ImGui::Spacing();
    ToggleColor("Name ESP", &hooks::espNameEnabled, hooks::espNameColor);
    Toggle("Health Bar", &hooks::espHealthBar);
    if (hooks::espHealthBar) {
      ImGui::Text("  Full HP");
      ImGui::SameLine();
      ColorEdit("hpStart", hooks::espHealthStartColor);
      ImGui::SameLine();
      ImGui::Text("Low HP");
      ImGui::SameLine();
      ColorEdit("hpEnd", hooks::espHealthEndColor);
    }
    ToggleColor("Armor Bar", &hooks::espArmorBar, hooks::espArmorColor);
    ToggleColor("Weapon ESP", &hooks::espWeaponEnabled, hooks::espWeaponColor);
    ToggleColor("Skeleton ESP", &hooks::espSkeletonEnabled,
                hooks::espSkeletonColor);
    ToggleColor("Head Circle", &hooks::espHeadCircle,
                hooks::espHeadCircleColor);
    Toggle("Weapon Icons", &hooks::espWeaponIcon);
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::SameLine(0, kPad);

  // ---- Right column ----
  ImGui::BeginChild("##vis_right", ImVec2(0, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Sound ESP");
  Toggle("Enable Sound ESP", &hooks::espSoundEnabled);
  if (hooks::espSoundEnabled) {
    ImGui::SliderFloat("Duration (s)", &hooks::espSoundTime, 0.5f, 5.0f,
                        "%.1f");
  }

  ImGui::Spacing();
  SectionHeader("Radar");
  Toggle("Enable Radar Overlay", &hooks::radarEnabled);
  Toggle("Force HUD Radar", &hooks::radarForceHud);
  if (hooks::radarEnabled || hooks::radarForceHud) {
    Toggle("Outlines", &hooks::radarOutlines);
    Toggle("Player Dot", &hooks::radarPlayerDot);
    if (hooks::radarPlayerDot) {
      ImGui::Text("Dot Color");
      ImGui::SameLine();
      ColorEdit("dotCol", hooks::radarDotColor);
    }
    Toggle("Show Dropped Weapons", &hooks::radarDroppedWeapons);
    Toggle("Show Dropped Grenades", &hooks::radarDroppedGrenades);
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();
}

// ============================================================================
// Tab: World ESP
// ============================================================================
static void RenderWorldESPTab() {
  float colW = (ImGui::GetContentRegionAvail().x - kPad) * 0.5f;

  ImGui::BeginChild("##world_left", ImVec2(colW, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Bomb Information");
  Toggle("Bomb Timer", &hooks::bombTimerEnabled);
  Toggle("Defuse Circle", &hooks::bombDefuseCircle);
  ToggleColor("Bomb Location ESP", &hooks::bombLocation,
              hooks::bombLocationColor);

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::SameLine(0, kPad);

  ImGui::BeginChild("##world_right", ImVec2(0, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Dropped Items");
  ToggleColor("Dropped Items", &hooks::droppedItemsEnabled,
              hooks::droppedItemsColor);
  Toggle("Dropped Weapons", &hooks::droppedWeaponsEnabled);
  Toggle("Dropped Grenades", &hooks::droppedGrenadesEnabled);
  Toggle("Molotov Fire ESP", &hooks::molotovFireEnabled);

  ImGui::PopStyleVar();
  ImGui::EndChild();
}

// ============================================================================
// Tab: Misc
// ============================================================================
static void RenderMiscTab() {
  float colW = (ImGui::GetContentRegionAvail().x - kPad) * 0.5f;

  ImGui::BeginChild("##misc_left", ImVec2(colW, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Hitmarker");
  Toggle("Enable", &hooks::hitmarkerEnabled);
  if (hooks::hitmarkerEnabled) {
    ImGui::Combo("Sound", &hooks::hitmarkerSound,
                 "None\0COD Tick\0Bell\0Minecraft\0");
    ImGui::SliderFloat("Volume", &hooks::hitmarkerVolume, 0.0f, 100.0f,
                        "%.0f%%");
  }

  ImGui::Spacing();
  SectionHeader("Crosshair");
  Toggle("Recoil Crosshair", &hooks::recoilCrosshair);

  ImGui::Spacing();
  SectionHeader("Movement");
  Toggle("SnapTap (Counter-Strafe)", &hooks::snaptapEnabled);
  if (hooks::snaptapEnabled) {
    ImGui::TextColored(kWarning, "  ! Ban risk - use at own risk");
  }

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::SameLine(0, kPad);

  ImGui::BeginChild("##misc_right", ImVec2(0, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("General");
  Toggle("Deathmatch Mode", &hooks::ignoreTeam);
  Toggle("Streamproof", &hooks::streamproof);




  if (hooks::customGameFov > 0.1f) {
    ImGui::SliderFloat("Game FOV", &hooks::customGameFov, 60.0f, 130.0f,
                        "%.0f");
  } else {
    ImGui::SliderFloat("Game FOV", &hooks::customGameFov, 0.0f, 130.0f,
                        "%.0f");
  }

  ImGui::Spacing();
  SectionHeader("Spectators");
  Toggle("Spectator List", &hooks::spectatorList);
  if (hooks::spectatorList)
    Toggle("Verbose", &hooks::spectatorListVerbose);

  ImGui::Spacing();
  SectionHeader("Menu");
  ImGui::Text("Menu Key");
  KeyBind("menuKey", &hooks::menuKeyBind, &hooks::waitingForMenuKey);

  ImGui::PopStyleVar();
  ImGui::EndChild();
}

// ============================================================================
// Tab: Grenade Helper
// ============================================================================
static void RenderGrenadeHelperTab() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Grenade Helper");
  Toggle("Enable", &hooks::grenadeHelperEnabled);

  if (hooks::grenadeHelperEnabled) {
    ImGui::Combo("Filter", &hooks::grenadeFilter,
                 "All\0Smoke\0Flash\0Molotov\0HE\0");
    ImGui::SliderFloat("Max Distance", &hooks::grenadeDistance, 100.0f,
                        2000.0f, "%.0f units");
    Toggle("Aim Assist", &hooks::grenadeAimAssist);
    if (hooks::grenadeAimAssist) {
      ImGui::SliderFloat("Assist Strength", &hooks::grenadeAimAssistStrength,
                         0.01f, 0.25f, "%.2f");
    }

    ImGui::Text("Marker Color");
    ImGui::SameLine();
    ColorEdit("nadeCol", hooks::grenadeColor);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kTextDim, "Map: %s", grenadehelper::CurrentMapName());
    ImGui::TextColored(kTextDim, "Loaded spots: %d", grenadehelper::LoadedSpotCount());
    ImGui::TextColored(kTextDim, "Data file: grenades\\<map>.csv");
    ImGui::TextColored(kTextDim, "Stand in position and aim to save new spots.");

    ImGui::Spacing();
    if (ImGui::Button("Save Current Position", ImVec2(-1, 30))) {
      grenadehelper::RequestSaveCurrentSpot();
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Refresh Spots from Cloud", ImVec2(-1, 24))) {
      grenadehelper::RefreshCurrentMap();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Manage Saved Spots")) {
      auto& spots = grenadehelper::GetSpots();
      if (spots.empty()) {
        ImGui::TextColored(kTextDim, "No spots loaded.");
      } else {
        ImGui::BeginChild("##spots_list", ImVec2(-1, 150), true);
        static int selectedSpot = -1;
        if (selectedSpot >= spots.size()) selectedSpot = -1;

        for (int i = 0; i < spots.size(); i++) {
          char label[128];
          snprintf(label, sizeof(label), "[%s] %s##%d", 
                   (spots[i].type == 1 ? "Smoke" : spots[i].type == 2 ? "Flash" : spots[i].type == 3 ? "Molotov" : spots[i].type == 4 ? "HE" : "Unknown"), 
                   spots[i].name, i);
          
          if (ImGui::Selectable(label, selectedSpot == i)) {
            selectedSpot = i;
          }
        }
        ImGui::EndChild();

        if (selectedSpot != -1 && selectedSpot < spots.size()) {
          auto& spot = spots[selectedSpot];
          ImGui::InputText("Name", spot.name, sizeof(spot.name));
          
          int currentType = spot.type - 1;
          if (currentType < 0) currentType = 0;
          if (ImGui::Combo("Type", &currentType, "Smoke\0Flash\0Molotov\0HE\0")) {
            spot.type = currentType + 1;
          }

          if (ImGui::Button("Save Changes", ImVec2(120, 0))) {
            grenadehelper::SaveAllSpots();
          }
          ImGui::SameLine();
          if (ImGui::Button("Delete Spot", ImVec2(120, 0))) {
            grenadehelper::DeleteSpot(selectedSpot);
            selectedSpot = -1;
          }
        }
      }
    }
  }

  ImGui::PopStyleVar();
}

// ============================================================================
// Tab: Transcript
// ============================================================================
static void RenderTranscriptTab() {
  float colW = (ImGui::GetContentRegionAvail().x - kPad) * 0.5f;

  ImGui::BeginChild("##transcript_left", ImVec2(colW, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Text Translation");
  Toggle("Enable Transcript", &hooks::transcriptEnabled);
  ImGui::Combo("Target Language", &hooks::transcriptTargetLang,
               "English\0Hindi\0Arabic\0Spanish\0Portuguese\0");
  Toggle("Translate Text Chat", &hooks::transcriptTranslateText);
  Toggle("Translate ASCII Chat", &hooks::transcriptTranslateAscii);
  Toggle("Show Original", &hooks::transcriptShowOriginal);

  ImGui::Spacing();
  ImGui::TextColored(kTextDim, "Status: %s", transcript::Status());

  ImGui::PopStyleVar();
  ImGui::EndChild();

  ImGui::SameLine(0, kPad);

  ImGui::BeginChild("##transcript_right", ImVec2(0, -1), false);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Voice");
  Toggle("Voice Transcription", &hooks::transcriptVoiceEnabled);
  Toggle("Translate Voice", &hooks::transcriptTranslateVoice);
  ImGui::SliderFloat("Voice Gate", &hooks::transcriptVoiceRmsThreshold, 0.002f,
                      0.080f, "%.3f");

  ImGui::Spacing();
  SectionHeader("Panel");
  if (ImGui::SliderFloat("X", &hooks::transcriptPanelX, 0.0f, 2500.0f, "%.0f"))
    ImGui::SetWindowPos("AI Transcript", ImVec2(hooks::transcriptPanelX, hooks::transcriptPanelY));
  if (ImGui::SliderFloat("Y", &hooks::transcriptPanelY, 0.0f, 1400.0f, "%.0f"))
    ImGui::SetWindowPos("AI Transcript", ImVec2(hooks::transcriptPanelX, hooks::transcriptPanelY));
  if (ImGui::SliderFloat("Width", &hooks::transcriptPanelW, 260.0f, 900.0f, "%.0f"))
    ImGui::SetWindowSize("AI Transcript", ImVec2(hooks::transcriptPanelW, hooks::transcriptPanelH));
  if (ImGui::SliderFloat("Height", &hooks::transcriptPanelH, 120.0f, 500.0f, "%.0f"))
    ImGui::SetWindowSize("AI Transcript", ImVec2(hooks::transcriptPanelW, hooks::transcriptPanelH));
  ImGui::SliderInt("Max Lines", &hooks::transcriptMaxMessages, 3, 40);

  ImGui::PopStyleVar();
  ImGui::EndChild();
}

// ============================================================================
// Tab: Config
// ============================================================================
static char configName[64] = "default";

static void RenderConfigTab() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

  SectionHeader("Configuration");

  ImGui::Text("Config Name");
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##cfgName", configName, sizeof(configName));

  ImGui::Spacing();
  float btnW = (ImGui::GetContentRegionAvail().x - 8) * 0.5f;

  if (ImGui::Button("Save Config", ImVec2(btnW, 32)))
    config::Save();
  ImGui::SameLine(0, 8);
  if (ImGui::Button("Load Config", ImVec2(btnW, 32)))
    config::Load();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::TextColored(kTextDim, "Config is saved to cs2_config.ini");
  ImGui::TextColored(kTextDim, "in the game directory.");

  ImGui::PopStyleVar();
}

// ============================================================================
// Main Menu Render
// ============================================================================
void menu::Render() {
  if (!hooks::showMenu)
    return;

  ImGui::SetNextWindowSize(ImVec2(kMenuW, kMenuH), ImGuiCond_FirstUseEver);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##CS2Menu", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();

  // ---- Sidebar ----
  RenderSidebar();
  ImGui::SameLine();

  // ---- Content area ----
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
  ImGui::BeginChild("##content", ImVec2(0, -1), false);

  switch (menu::currentTab) {
  case Tab_Aimbot:
    RenderAimbotTab();
    break;
  case Tab_Visuals:
    RenderVisualsTab();
    break;
  case Tab_WorldESP:
    RenderWorldESPTab();
    break;
  case Tab_Misc:
    RenderMiscTab();
    break;
  case Tab_GrenadeHelper:
    RenderGrenadeHelperTab();
    break;
  case Tab_Transcript:
    RenderTranscriptTab();
    break;
  case Tab_Config:
    RenderConfigTab();
    break;
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();

  ImGui::End();
}
