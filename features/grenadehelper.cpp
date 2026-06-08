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
  
  // Download synchronously. This is extremely fast for a 1KB text file and only runs once per map load.
  HRESULT hr = URLDownloadToFileA(NULL, url, path, 0, NULL);
  if (FAILED(hr)) {
      // If download fails (no internet, or file doesn't exist on repo), we just fall back to the existing local file.
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

  current.x += delta.x * 0.18f;
  current.y += delta.y * 0.18f;
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
  const GrenadeSpot *bestSpot = nullptr;
  float bestDistance = FLT_MAX;

  for (const GrenadeSpot &spot : g_spots) {
    if (!FilterMatches(spot.type))
      continue;
    if (heldType != 0 && spot.type != 0 && spot.type != heldType)
      continue;

    float dist = (spot.origin - localOrigin).Length();
    if (dist > hooks::grenadeDistance)
      continue;

    Vector3 screen;
    if (!WorldToScreen(spot.origin + Vector3(0, 0, 6.0f), viewMatrix, displaySize.x,
                       displaySize.y, screen)) {
      continue;
    }

    draw->AddCircle(ImVec2(screen.x, screen.y), 12.0f, color, 20, 2.0f);
    char label[128];
    snprintf(label, sizeof(label), "%s [%s] %.0fm", spot.name, TypeName(spot.type),
             dist * 0.0254f);
    ImVec2 size = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(screen.x - size.x * 0.5f, screen.y - 22.0f), color, label);

    if (dist < bestDistance) {
      bestDistance = dist;
      bestSpot = &spot;
    }
  }

  ApplyAimAssist(clientBase, bestSpot, bestDistance);
}
