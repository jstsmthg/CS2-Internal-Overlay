#include "pch.h"
#include <cctype>
#include <vector>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

namespace {

struct GrenadeSpot {
  char name[64];
  int type;
  Vector3 origin;
  Vector3 angles;
};

static std::vector<GrenadeSpot> g_spots;
static char g_loadedMap[64] = "";
static bool g_saveRequested = false;

static const char *TypeName(int type) {
  switch (type) {
  case 1: return "smoke";
  case 2: return "flash";
  case 3: return "molotov";
  case 4: return "he";
  default: return "all";
  }
}

static int DetectHeldGrenadeType(uintptr_t localPawn, uintptr_t entityList) {
  uintptr_t weaponServices =
      *(uintptr_t *)(localPawn + schemas::C_BasePlayerPawn::m_pWeaponServices);
  if (!weaponServices)
    return 0;

  uint32_t activeHandle =
      *(uint32_t *)(weaponServices + schemas::CPlayer_WeaponServices::m_hActiveWeapon);
  if (!activeHandle || activeHandle == 0xFFFFFFFF)
    return 0;

  uintptr_t listEntry =
      *(uintptr_t *)(entityList + 0x10 + 8 * ((activeHandle & 0x7FFF) >> 9));
  if (!listEntry)
    return 0;

  uintptr_t weaponEntity =
      *(uintptr_t *)(listEntry + 0x70 * ((activeHandle & 0x7FFF) & 0x1FF));
  if (!weaponEntity)
    return 0;

  uint16_t itemDef =
      *(uint16_t *)(weaponEntity + schemas::C_EconEntity::m_AttributeManager +
                    schemas::C_AttributeContainer::m_Item +
                    schemas::C_EconItemView::m_iItemDefinitionIndex);
  switch (itemDef) {
  case 43: return 2;
  case 44: return 4;
  case 45: return 1;
  case 46:
  case 48: return 3;
  default: return 0;
  }
}

static bool ReadCurrentMapName(uintptr_t clientBase, char *out, size_t outSize) {
  if (!out || outSize == 0)
    return false;
  out[0] = '\0';

  uintptr_t globalVarsCandidates[2] = {
      *(uintptr_t *)(clientBase + offsets::dwGlobalVars),
      clientBase + offsets::dwGlobalVars,
  };

  for (uintptr_t base : globalVarsCandidates) {
    if (!base || base < 0x10000)
      continue;
    __try {
      uintptr_t mapNamePtr = *(uintptr_t *)(base + 0x188);
      if (!mapNamePtr || mapNamePtr < 0x10000)
        continue;

      strncpy_s(out, outSize, (const char *)mapNamePtr, _TRUNCATE);
      if (out[0] != '\0')
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      continue;
    }
  }

  return false;
}

static void SanitizeMapName(char *name) {
  for (char *p = name; *p; ++p) {
    if (!std::isalnum((unsigned char)*p) && *p != '_' && *p != '-')
      *p = '_';
  }
}

static bool GetMapFilePath(char *path, size_t pathSize) {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return false;

  char mapName[64];
  if (!ReadCurrentMapName(clientBase, mapName, sizeof(mapName)))
    return false;
  SanitizeMapName(mapName);

  CreateDirectoryA("grenades", nullptr);
  snprintf(path, pathSize, "grenades\\%s.csv", mapName);
  return true;
}

static void LoadSpotsForCurrentMap() {
  char path[MAX_PATH];
  char mapName[64];
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase || !ReadCurrentMapName(clientBase, mapName, sizeof(mapName))) {
    g_loadedMap[0] = '\0';
    g_spots.clear();
    return;
  }
  SanitizeMapName(mapName);

  if (strcmp(g_loadedMap, mapName) == 0)
    return;

  strncpy_s(g_loadedMap, sizeof(g_loadedMap), mapName, _TRUNCATE);
  g_spots.clear();

  if (!GetMapFilePath(path, sizeof(path)))
    return;

  // Attempt to download the latest grenade spots for this map from a GitHub raw URL.
  // We use the new jstsmthg repo. Make sure to upload CSV files to a 'grenades' folder in the repo.
  // By using 0 for the dwFlags, it will overwrite the file if an update exists or use cache.
  char url[256];
  snprintf(url, sizeof(url), "https://raw.githubusercontent.com/jstsmthg/CS2-Internal-Overlay/master/grenades/%s.csv", mapName);
  
  // Download synchronously only if the file doesn't exist locally
  if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
      HRESULT hr = URLDownloadToFileA(NULL, url, path, 0, NULL);
      if (FAILED(hr)) {
          // If download fails, we just fall back
      }
  }

  FILE *f = nullptr;
  fopen_s(&f, path, "r");
  if (!f)
    return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
      continue;

    GrenadeSpot spot = {};
    char typeName[32] = {};
    if (sscanf_s(line, "%63[^;];%31[^;];%f;%f;%f;%f;%f",
                 spot.name, (unsigned)_countof(spot.name),
                 typeName, (unsigned)_countof(typeName),
                 &spot.origin.x, &spot.origin.y, &spot.origin.z,
                 &spot.angles.x, &spot.angles.y) != 7) {
      continue;
    }

    if (_stricmp(typeName, "smoke") == 0) spot.type = 1;
    else if (_stricmp(typeName, "flash") == 0) spot.type = 2;
    else if (_stricmp(typeName, "molotov") == 0) spot.type = 3;
    else if (_stricmp(typeName, "he") == 0) spot.type = 4;
    else spot.type = 0;

    g_spots.push_back(spot);
  }

  fclose(f);
}

static void SaveCurrentSpotIfRequested() {
  if (!g_saveRequested)
    return;
  g_saveRequested = false;

  char path[MAX_PATH];
  if (!GetMapFilePath(path, sizeof(path)))
    return;

  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;
  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!localPawn || !entityList)
    return;

  uintptr_t sceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  if (!sceneNode)
    return;

  Vector3 origin =
      *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
  Vector3 angles = *(Vector3 *)(clientBase + offsets::dwViewAngles);
  int heldType = DetectHeldGrenadeType(localPawn, entityList);
  if (heldType == 0)
    heldType = hooks::grenadeFilter;
  if (heldType == 0)
    heldType = 1;

  FILE *f = nullptr;
  fopen_s(&f, path, "a");
  if (!f)
    return;

  SYSTEMTIME st;
  GetLocalTime(&st);
  char name[64];
  snprintf(name, sizeof(name), "%s_%02d%02d_%02d%02d%02d",
           TypeName(heldType), st.wMonth, st.wDay, st.wHour, st.wMinute,
           st.wSecond);
  fprintf(f, "%s;%s;%.3f;%.3f;%.3f;%.3f;%.3f\n",
          name, TypeName(heldType), origin.x, origin.y, origin.z,
          angles.x, angles.y);
  fclose(f);

  g_loadedMap[0] = '\0';
}

static bool FilterMatches(int spotType) {
  return hooks::grenadeFilter == 0 || hooks::grenadeFilter == spotType;
}

static Vector3 AngleToDirection(float pitch, float yaw) {
  float p = pitch * (3.14159265f / 180.0f);
  float y = yaw * (3.14159265f / 180.0f);
  return { cosf(p) * cosf(y), cosf(p) * sinf(y), -sinf(p) };
}

static void ApplyAimAssist(uintptr_t clientBase, const GrenadeSpot *bestSpot,
                           float distance) {
  if (!hooks::grenadeAimAssist || !bestSpot || distance > 85.0f || hooks::showMenu)
    return;

  Vector3 current = *(Vector3 *)(clientBase + offsets::dwViewAngles);
  Vector3 target = {bestSpot->angles.x, bestSpot->angles.y, 0.0f};
  Vector3 delta;
  delta.x = NormalizePitch(target.x - current.x);
  delta.y = NormalizeYaw(target.y - current.y);
  delta.z = 0.0f;

  current.x += delta.x * hooks::grenadeAimAssistStrength;
  current.y += delta.y * hooks::grenadeAimAssistStrength;
  current = ClampAngles(current);
  *(Vector3 *)(clientBase + offsets::dwViewAngles) = current;
}

} // namespace

void grenadehelper::RequestSaveCurrentSpot() {
  g_saveRequested = true;
}

const char *grenadehelper::CurrentMapName() {
  return g_loadedMap[0] ? g_loadedMap : "(unknown)";
}

int grenadehelper::LoadedSpotCount() {
  return (int)g_spots.size();
}

void grenadehelper::Render() {
  if (!hooks::grenadeHelperEnabled)
    return;

  SaveCurrentSpotIfRequested();
  LoadSpotsForCurrentMap();

  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!localPawn || !entityList)
    return;

  uintptr_t sceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  if (!sceneNode)
    return;

  Vector3 localOrigin =
      *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
  view_matrix_t &viewMatrix =
      *(view_matrix_t *)(clientBase + offsets::dwViewMatrix);
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  ImDrawList *draw = ImGui::GetBackgroundDrawList();
  ImU32 color = ImGui::ColorConvertFloat4ToU32(
      ImVec4(hooks::grenadeColor[0], hooks::grenadeColor[1], hooks::grenadeColor[2],
             hooks::grenadeColor[3]));

  int heldType = DetectHeldGrenadeType(localPawn, entityList);
  
  struct DrawnText { ImVec2 pos; };
  std::vector<DrawnText> drawnTexts;

  const GrenadeSpot *bestAimSpot = nullptr;
  float bestFov = FLT_MAX;
  float minPhysicalDist = FLT_MAX;
  Vector3 currentAngles = *(Vector3 *)(clientBase + offsets::dwViewAngles);

  // ── Pass 1: find the single closest qualifying spot ──
  int closestIdx = -1;
  float closestDist = FLT_MAX;
  for (int i = 0; i < (int)g_spots.size(); ++i) {
      const GrenadeSpot& spot = g_spots[i];
      if (!FilterMatches(spot.type)) continue;
      if (heldType != 0 && spot.type != 0 && spot.type != heldType) continue;
      
      float dist = (spot.origin - localOrigin).Length();
      if (dist <= 120.0f && dist < closestDist) {
          closestDist = dist;
      }
  }

  // ── Pass 2: render all visible spots ──
  for (int i = 0; i < (int)g_spots.size(); ++i) {
    const GrenadeSpot &spot = g_spots[i];
    if (!FilterMatches(spot.type))
      continue;
    if (heldType != 0 && spot.type != 0 && spot.type != heldType)
      continue;

    float dist = (spot.origin - localOrigin).Length();
    float distMeters = dist * 0.01905f;
    if (dist > hooks::grenadeDistance)
      continue;

    if (dist < minPhysicalDist) 
      minPhysicalDist = dist;

    // Activate ALL spots that share the exact same physical origin as the closest spot
    bool isActive = false;
    if (closestDist != FLT_MAX) {
      isActive = (dist <= closestDist + 5.0f);
    }

    Vector3 screen;
    bool onScreen = WorldToScreen(spot.origin + Vector3(0, 0, 6.0f), viewMatrix, displaySize.x, displaySize.y, screen);

    if (isActive) {
      if (onScreen) {
        // Draw a noticeable dot on the ground instead of a massive empty ring
        draw->AddCircleFilled(ImVec2(screen.x, screen.y), 12.0f, IM_COL32(0, 0, 0, 150));
        draw->AddCircle(ImVec2(screen.x, screen.y), 12.0f, color, 24, 2.0f);
        
        // Render name tag above the dot
        char displayName[128];
        strncpy_s(displayName, sizeof(displayName), spot.name, _TRUNCATE);
        char* jt = strstr(displayName, "(JT)");
        if (jt) *jt = '\0';
        for (int k = (int)strlen(displayName) - 1; k >= 0 && displayName[k] == ' '; k--) {
            displayName[k] = '\0';
        }
        
        ImVec2 size = ImGui::CalcTextSize(displayName);
        ImVec2 textPos(screen.x, screen.y - 25.0f);
        ImVec2 bgMin(textPos.x - size.x * 0.5f - 4.0f, textPos.y - size.y * 0.5f - 2.0f);
        ImVec2 bgMax(textPos.x + size.x * 0.5f + 4.0f, textPos.y + size.y * 0.5f + 2.0f);
        draw->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 4.0f);
        draw->AddText(ImVec2(textPos.x - size.x * 0.5f, textPos.y - size.y * 0.5f), color, displayName);
      }

      Vector3 aimDir = AngleToDirection(spot.angles.x, spot.angles.y);
      Vector3 aimPoint = spot.origin + Vector3(0, 0, 64.0f) + aimDir * 800.0f;
      Vector3 aimScreen;
      
      if (WorldToScreen(aimPoint, viewMatrix, displaySize.x, displaySize.y, aimScreen)) {
        draw->AddCircle(ImVec2(aimScreen.x, aimScreen.y), 10.0f, color, 16, 2.0f);
        draw->AddLine(ImVec2(aimScreen.x - 18.0f, aimScreen.y), ImVec2(aimScreen.x - 5.0f, aimScreen.y), color, 2.0f);
        draw->AddLine(ImVec2(aimScreen.x + 5.0f, aimScreen.y), ImVec2(aimScreen.x + 18.0f, aimScreen.y), color, 2.0f);
        draw->AddLine(ImVec2(aimScreen.x, aimScreen.y - 18.0f), ImVec2(aimScreen.x, aimScreen.y - 5.0f), color, 2.0f);
        draw->AddLine(ImVec2(aimScreen.x, aimScreen.y + 5.0f), ImVec2(aimScreen.x, aimScreen.y + 18.0f), color, 2.0f);

        char instruction[64] = "Left Click Throw";
        char displayName[128];
        strncpy_s(displayName, sizeof(displayName), spot.name, _TRUNCATE);
        
        char* jt = strstr(displayName, "(JT)");
        if (jt) {
            *jt = '\0';
            strcpy_s(instruction, sizeof(instruction), "Jump Throw");
        }
        for (int k = (int)strlen(displayName) - 1; k >= 0 && displayName[k] == ' '; k--) {
            displayName[k] = '\0';
        }

        char label[256];
        snprintf(label, sizeof(label), "%s\n%s", displayName, instruction);
        ImVec2 size = ImGui::CalcTextSize(label);
        ImVec2 textPos(aimScreen.x, aimScreen.y - 40.0f);

        bool collision = true;
        while (collision) {
          collision = false;
          for (const auto& dt : drawnTexts) {
            if (std::abs(dt.pos.x - textPos.x) < size.x + 10.0f && std::abs(dt.pos.y - textPos.y) < size.y + 10.0f) {
              textPos.y -= (size.y + 10.0f);
              collision = true;
              break;
            }
          }
        }
        drawnTexts.push_back({textPos});

        ImVec2 bgMin(textPos.x - size.x * 0.5f - 6.0f, textPos.y - size.y * 0.5f - 4.0f);
        ImVec2 bgMax(textPos.x + size.x * 0.5f + 6.0f, textPos.y + size.y * 0.5f + 4.0f);
        draw->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 200), 6.0f);
        draw->AddText(ImVec2(textPos.x - size.x * 0.5f, textPos.y - size.y * 0.5f), color, label);
      }

      Vector3 delta;
      delta.x = NormalizePitch(spot.angles.x - currentAngles.x);
      delta.y = NormalizeYaw(spot.angles.y - currentAngles.y);
      float fov = std::sqrt(delta.x*delta.x + delta.y*delta.y);
      if (fov < bestFov) {
        bestFov = fov;
        bestAimSpot = &spot;
      }

    } else {
      if (!onScreen) continue;

      draw->AddCircleFilled(ImVec2(screen.x, screen.y), 4.0f, IM_COL32(0, 0, 0, 150));
      draw->AddCircle(ImVec2(screen.x, screen.y), 4.0f, color, 12, 2.0f);

      char displayName[128];
      strncpy_s(displayName, sizeof(displayName), spot.name, _TRUNCATE);
      char* jt = strstr(displayName, "(JT)");
      if (jt) *jt = '\0';
      for (int k = (int)strlen(displayName) - 1; k >= 0 && displayName[k] == ' '; k--) {
          displayName[k] = '\0';
      }

      char label[128];
      snprintf(label, sizeof(label), "%s [%.1fm]", displayName, distMeters);
      ImVec2 size = ImGui::CalcTextSize(label);
      ImVec2 textPos(screen.x, screen.y - 20.0f);

      bool collision = true;
      while (collision) {
        collision = false;
        for (const auto& dt : drawnTexts) {
          if (std::abs(dt.pos.x - textPos.x) < size.x + 10.0f && std::abs(dt.pos.y - textPos.y) < size.y + 10.0f) {
            textPos.y -= (size.y + 10.0f);
            collision = true;
            break;
          }
        }
      }
      drawnTexts.push_back({textPos});

      ImVec2 bgMin(textPos.x - size.x * 0.5f - 4.0f, textPos.y - size.y * 0.5f - 2.0f);
      ImVec2 bgMax(textPos.x + size.x * 0.5f + 4.0f, textPos.y + size.y * 0.5f + 2.0f);
      draw->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 4.0f);
      draw->AddText(ImVec2(textPos.x - size.x * 0.5f, textPos.y - size.y * 0.5f), color, label);
    }
  }

  ApplyAimAssist(clientBase, bestAimSpot, minPhysicalDist);
}
