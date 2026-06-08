#include "pch.h"
#include <algorithm>

namespace {

struct RadarPoint {
  Vector3 origin;
  int team;
  bool isTeammate;
};

struct WorldItem {
  Vector3 origin;
  bool grenade;
};

static constexpr float kRadarRange = 1800.0f;
static constexpr float kRadarSize = 220.0f;
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

static uint16_t GetItemDefIndex(uintptr_t entity) {
  if (!entity)
    return 0;
  return *(uint16_t *)(entity + schemas::C_EconEntity::m_AttributeManager +
                       schemas::C_AttributeContainer::m_Item +
                       schemas::C_EconItemView::m_iItemDefinitionIndex);
}

static bool IsKnownWeaponDef(uint16_t itemDef) {
  switch (itemDef) {
  case 1: case 2: case 3: case 4: case 7: case 8: case 9: case 10:
  case 11: case 13: case 14: case 16: case 17: case 19: case 23: case 24:
  case 25: case 26: case 27: case 28: case 29: case 30: case 32: case 33:
  case 34: case 35: case 36: case 38: case 39: case 40: case 60: case 61:
  case 63: case 64:
    return true;
  default:
    return false;
  }
}

static bool IsGrenadeDef(uint16_t itemDef) {
  switch (itemDef) {
  case 43:
  case 44:
  case 45:
  case 46:
  case 47:
  case 48:
  case 49:
    return true;
  default:
    return false;
  }
}

static bool IsWeaponDef(uint16_t itemDef) {
  return IsKnownWeaponDef(itemDef);
}

static void ForceHudRadar(uintptr_t entityList, uintptr_t localPawn) {
  if (!hooks::radarForceHud || !entityList || !localPawn)
    return;

  int localTeam = *(uint8_t *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);

  for (int i = 1; i <= 64; i++) {
    uintptr_t controller = ResolveIndex(entityList, i);
    if (!controller)
      continue;

    uint32_t pawnHandle =
        *(uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
    uintptr_t pawn = ResolveHandle(entityList, pawnHandle);
    if (!pawn || pawn == localPawn)
      continue;

    int health = *(int *)(pawn + schemas::C_BaseEntity::m_iHealth);
    int team = *(uint8_t *)(pawn + schemas::C_BaseEntity::m_iTeamNum);
    if (health <= 0 || team == localTeam)
      continue;

    *(bool *)(pawn + schemas::C_CSPlayerPawn::m_entitySpottedState +
              schemas::EntitySpottedState_t::m_bSpotted) = true;
  }
}

static ImU32 ToU32(const float *rgba) {
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

static void CollectPlayers(uintptr_t entityList, uintptr_t localPawn,
                           Vector3 localOrigin,
                           RadarPoint *points, int &count) {
  count = 0;
  int localTeam = *(uint8_t *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);

  for (int i = 1; i <= 64 && count < 64; i++) {
    uintptr_t controller = ResolveIndex(entityList, i);
    if (!controller)
      continue;

    uint32_t pawnHandle =
        *(uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
    uintptr_t pawn = ResolveHandle(entityList, pawnHandle);
    if (!pawn || pawn == localPawn)
      continue;

    int health = *(int *)(pawn + schemas::C_BaseEntity::m_iHealth);
    if (health <= 0)
      continue;

    uintptr_t sceneNode =
        *(uintptr_t *)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode)
      continue;

    Vector3 origin =
        *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
    if ((origin - localOrigin).Length2D() > 6000.0f)
      continue;

    int team = *(uint8_t *)(pawn + schemas::C_BaseEntity::m_iTeamNum);
    points[count++] = {origin, team, team == localTeam};
  }
}

static void CollectWorldItems(uintptr_t entityList, uintptr_t localOriginEntity,
                              const Vector3 &localOrigin, WorldItem *items,
                              int &count) {
  count = 0;
  (void)localOriginEntity;

  for (int i = 65; i < kMaxWorldEntities && count < 128; i++) {
    uintptr_t entity = ResolveIndex(entityList, i);
    if (!entity)
      continue;

    uint32_t ownerHandle =
        *(uint32_t *)(entity + schemas::C_BaseEntity::m_hOwnerEntity);
    if (ownerHandle && ownerHandle != 0xFFFFFFFF)
      continue;

    uint16_t itemDef = GetItemDefIndex(entity);
    bool grenade = IsGrenadeDef(itemDef);
    bool weapon = IsWeaponDef(itemDef);

    if ((grenade && !hooks::radarDroppedGrenades) ||
        (weapon && !hooks::radarDroppedWeapons) || (!grenade && !weapon)) {
      continue;
    }

    uintptr_t sceneNode =
        *(uintptr_t *)(entity + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode)
      continue;

    Vector3 origin =
        *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
    if ((origin - localOrigin).Length2D() > 6000.0f)
      continue;

    items[count++] = {origin, grenade};
  }
}

} // namespace

void radar::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  ForceHudRadar(entityList, localPawn);
  if (!hooks::radarEnabled)
    return;

  uintptr_t localSceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  if (!localSceneNode)
    return;

  Vector3 localOrigin =
      *(Vector3 *)(localSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
  Vector3 viewAngles = *(Vector3 *)(clientBase + offsets::dwViewAngles);

  RadarPoint players[64] = {};
  int playerCount = 0;
  CollectPlayers(entityList, localPawn, localOrigin, players, playerCount);

  WorldItem items[128] = {};
  int itemCount = 0;
  CollectWorldItems(entityList, localPawn, localOrigin, items, itemCount);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                           ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoInputs;
  ImGui::SetNextWindowSize(ImVec2(kRadarSize, kRadarSize), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.30f);
  ImGui::Begin("##radar_overlay", nullptr, flags);

  ImVec2 topLeft = ImGui::GetCursorScreenPos();
  ImVec2 size = ImGui::GetContentRegionAvail();
  float side = (std::min)(size.x, size.y);
  side = side > 40.0f ? side : size.x;

  ImDrawList *draw = ImGui::GetWindowDrawList();
  ImVec2 center(topLeft.x + side * 0.5f, topLeft.y + side * 0.5f);
  float radius = side * 0.5f - 8.0f;

  ImU32 bgCol = IM_COL32(10, 16, 22, 210);
  ImU32 lineCol = IM_COL32(0, 212, 255, 80);
  draw->AddRectFilled(topLeft, ImVec2(topLeft.x + side, topLeft.y + side), bgCol,
                      6.0f);

  if (hooks::radarOutlines) {
    draw->AddRect(topLeft, ImVec2(topLeft.x + side, topLeft.y + side),
                  IM_COL32(0, 212, 255, 180), 6.0f, 0, 1.0f);
    draw->AddLine(ImVec2(center.x, topLeft.y + 8.0f),
                  ImVec2(center.x, topLeft.y + side - 8.0f), lineCol, 1.0f);
    draw->AddLine(ImVec2(topLeft.x + 8.0f, center.y),
                  ImVec2(topLeft.x + side - 8.0f, center.y), lineCol, 1.0f);
  }

  const float yawRad = (-viewAngles.y + 90.0f) * (3.14159265f / 180.0f);
  const float cosYaw = std::cosf(yawRad);
  const float sinYaw = std::sinf(yawRad);
  const float scale = radius / kRadarRange;

  auto project = [&](const Vector3 &world) {
    Vector3 delta = world - localOrigin;
    float rx = delta.x * cosYaw - delta.y * sinYaw;
    float ry = delta.x * sinYaw + delta.y * cosYaw;
    rx = (std::clamp)(rx, -kRadarRange, kRadarRange);
    ry = (std::clamp)(ry, -kRadarRange, kRadarRange);
    return ImVec2(center.x + rx * scale, center.y + ry * scale);
  };

  if (hooks::radarDroppedWeapons || hooks::radarDroppedGrenades) {
    for (int i = 0; i < itemCount; i++) {
      ImVec2 p = project(items[i].origin);
      ImU32 col = items[i].grenade ? IM_COL32(255, 196, 64, 210)
                                   : IM_COL32(190, 190, 190, 210);
      draw->AddRectFilled(ImVec2(p.x - 2.0f, p.y - 2.0f),
                          ImVec2(p.x + 2.0f, p.y + 2.0f), col, 1.0f);
    }
  }

  if (hooks::radarPlayerDot) {
    ImU32 enemyCol = ToU32(hooks::radarDotColor);
    ImU32 teamCol = IM_COL32(64, 220, 128, 220);
    for (int i = 0; i < playerCount; i++) {
      ImVec2 p = project(players[i].origin);
      draw->AddCircleFilled(p, 3.5f, players[i].isTeammate ? teamCol : enemyCol);
    }
  }

  ImVec2 tri[3] = {
      ImVec2(center.x, center.y - 7.0f),
      ImVec2(center.x - 5.0f, center.y + 5.0f),
      ImVec2(center.x + 5.0f, center.y + 5.0f),
  };
  draw->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 255, 255, 240));

  ImGui::Dummy(ImVec2(side, side));
  ImGui::End();
}
