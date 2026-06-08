#include "pch.h"
#include <algorithm>

namespace {

static constexpr int kMaxWorldEntities = 2048;

static uintptr_t ResolveIndex(uintptr_t entityList, int index) {
  uintptr_t listEntry =
      *(uintptr_t *)(entityList + 0x10 + 8 * ((index & 0x7FFF) >> 9));
  if (!listEntry)
    return 0;
  return *(uintptr_t *)(listEntry + 0x70 * (index & 0x1FF));
}

static ImU32 ToU32(const float *rgba) {
  return ImGui::ColorConvertFloat4ToU32(
      ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
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

static const char *ItemLabel(uint16_t itemDef) {
  switch (itemDef) {
  case 1: return "Deagle";
  case 4: return "Glock";
  case 7: return "AK-47";
  case 9: return "AWP";
  case 16: return "M4A4";
  case 17: return "MAC-10";
  case 19: return "P90";
  case 23: return "MP5-SD";
  case 24: return "UMP-45";
  case 25: return "XM1014";
  case 26: return "PP-Bizon";
  case 27: return "MAG-7";
  case 28: return "Negev";
  case 29: return "Sawed-Off";
  case 30: return "Tec-9";
  case 32: return "P2000";
  case 33: return "MP7";
  case 34: return "MP9";
  case 35: return "Nova";
  case 36: return "P250";
  case 38: return "SCAR-20";
  case 39: return "SG 553";
  case 40: return "SSG 08";
  case 43: return "Flash";
  case 44: return "HE";
  case 45: return "Smoke";
  case 46: return "Molotov";
  case 47: return "Decoy";
  case 48: return "Incendiary";
  case 49: return "C4";
  case 60: return "M4A1-S";
  case 61: return "USP-S";
  default: return IsGrenadeDef(itemDef) ? "Grenade" : "Unknown";
  }
}

static void DrawWorldLabel(ImDrawList *drawList, const view_matrix_t &viewMatrix,
                           float screenW, float screenH, const Vector3 &worldPos,
                           ImU32 color, const char *label, float yOffset = 0.0f) {
  Vector3 screen;
  if (!WorldToScreen(worldPos, viewMatrix, screenW, screenH, screen))
    return;

  ImVec2 textSize = ImGui::CalcTextSize(label);
  ImVec2 pos(screen.x - textSize.x * 0.5f, screen.y + yOffset);
  drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 180),
                    label);
  drawList->AddText(pos, color, label);
}

static void RenderBombLocation(uintptr_t clientBase, ImDrawList *drawList,
                               const view_matrix_t &viewMatrix, float screenW,
                               float screenH) {
  if (!hooks::bombLocation)
    return;

  uintptr_t c4Entity = bombtimer::FindPlantedC4Entity();
  if (!c4Entity)
    return;

  bool ticking = *(bool *)(c4Entity + schemas::C_PlantedC4::m_bBombTicking);
  if (!ticking)
    return;

  uintptr_t sceneNode =
      *(uintptr_t *)(c4Entity + schemas::C_BaseEntity::m_pGameSceneNode);
  if (!sceneNode)
    return;

  Vector3 origin =
      *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
  Vector3 screen;
  if (!WorldToScreen(origin, viewMatrix, screenW, screenH, screen))
    return;

  int bombSite = *(int *)(c4Entity + schemas::C_PlantedC4::m_nBombSite);
  ImU32 color = ToU32(hooks::bombLocationColor);
  drawList->AddCircle(ImVec2(screen.x, screen.y), 12.0f, color, 24, 2.0f);

  char label[32];
  snprintf(label, sizeof(label), "BOMB %c", bombSite == 1 ? 'B' : 'A');
  DrawWorldLabel(drawList, viewMatrix, screenW, screenH, origin + Vector3(0, 0, 12.0f),
                 color, label, -18.0f);
}

static void RenderDroppedItems(uintptr_t entityList, ImDrawList *drawList,
                               const view_matrix_t &viewMatrix, float screenW,
                               float screenH, const Vector3 &localOrigin) {
  if (!hooks::droppedItemsEnabled && !hooks::droppedWeaponsEnabled &&
      !hooks::droppedGrenadesEnabled) {
    return;
  }

  ImU32 itemColor = ToU32(hooks::droppedItemsColor);
  for (int i = 65; i < kMaxWorldEntities; i++) {
    uintptr_t entity = ResolveIndex(entityList, i);
    if (!entity)
      continue;

    uint32_t ownerHandle =
        *(uint32_t *)(entity + schemas::C_BaseEntity::m_hOwnerEntity);
    if (ownerHandle && ownerHandle != 0xFFFFFFFF)
      continue;

    uint16_t itemDef = GetItemDefIndex(entity);
    bool isGrenade = IsGrenadeDef(itemDef);
    bool isWeapon = IsWeaponDef(itemDef);
    if (!isGrenade && !isWeapon)
      continue;

    if (isWeapon && !hooks::droppedWeaponsEnabled && !hooks::droppedItemsEnabled)
      continue;
    if (isGrenade && !hooks::droppedGrenadesEnabled && !hooks::droppedItemsEnabled)
      continue;

    uintptr_t sceneNode =
        *(uintptr_t *)(entity + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode)
      continue;

    Vector3 origin =
        *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
    float distanceMeters = (origin - localOrigin).Length() * 0.0254f;
    char label[96];
    snprintf(label, sizeof(label), "%s [%.0fm]", ItemLabel(itemDef), distanceMeters);
    DrawWorldLabel(drawList, viewMatrix, screenW, screenH,
                   origin + Vector3(0, 0, 6.0f), itemColor, label);
  }
}

static void RenderMolotovs(uintptr_t entityList, ImDrawList *drawList,
                           const view_matrix_t &viewMatrix, float screenW,
                           float screenH) {
  if (!hooks::molotovFireEnabled)
    return;

  for (int i = 65; i < kMaxWorldEntities; i++) {
    uintptr_t entity = ResolveIndex(entityList, i);
    if (!entity)
      continue;

    __try {
      int fireCount = *(int *)(entity + schemas::C_Inferno::m_fireCount);
      int fireTick = *(int *)(entity + schemas::C_Inferno::m_nFireEffectTickBegin);
      int infernoType = *(int *)(entity + schemas::C_Inferno::m_nInfernoType);
      int drawableCount = *(int *)(entity + schemas::C_Inferno::m_drawableCount);
      float lifetime = *(float *)(entity + schemas::C_Inferno::m_nFireLifetime);
      float maxHalfWidth = *(float *)(entity + schemas::C_Inferno::m_maxFireHalfWidth);
      float maxHeight = *(float *)(entity + schemas::C_Inferno::m_maxFireHeight);
      if (fireTick <= 0 || fireCount <= 0 || fireCount > 64 || lifetime <= 0.0f ||
          lifetime > 8.5f || infernoType < 0 || infernoType > 4 ||
          drawableCount <= 0 || drawableCount > 64 || maxHalfWidth < 8.0f ||
          maxHalfWidth > 96.0f || maxHeight < 8.0f || maxHeight > 96.0f) {
        continue;
      }

      uintptr_t sceneNode =
          *(uintptr_t *)(entity + schemas::C_BaseEntity::m_pGameSceneNode);
      if (!sceneNode)
        continue;

      Vector3 origin =
          *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
      ImU32 fireColor = IM_COL32(255, 110, 35, 210);

      Vector3 centroid = {};
      int validPoints = 0;
      float furthest = 0.0f;
      for (int j = 0; j < fireCount; j++) {
        bool burning =
            *(bool *)(entity + schemas::C_Inferno::m_bFireIsBurning + j);
        if (!burning)
          continue;

        Vector3 firePos =
            *(Vector3 *)(entity + schemas::C_Inferno::m_firePositions +
                         (sizeof(Vector3) * j));
        float pointDistance = (firePos - origin).Length();
        if (pointDistance > 180.0f)
          continue;
        centroid = centroid + firePos;
        validPoints++;
      }

      if (validPoints < 4)
        continue;

      centroid = centroid * (1.0f / (float)validPoints);
      for (int j = 0; j < fireCount; j++) {
        bool burning =
            *(bool *)(entity + schemas::C_Inferno::m_bFireIsBurning + j);
        if (!burning)
          continue;

        Vector3 firePos =
            *(Vector3 *)(entity + schemas::C_Inferno::m_firePositions +
                         (sizeof(Vector3) * j));
        float distFromCentroid = (firePos - centroid).Length();
        if (distFromCentroid > furthest)
          furthest = distFromCentroid;
      }

      Vector3 screen;
      if (!WorldToScreen(centroid, viewMatrix, screenW, screenH, screen))
        continue;

      float radius = std::clamp(furthest * 0.28f, 16.0f, 44.0f);
      drawList->AddCircle(ImVec2(screen.x, screen.y), radius, fireColor, 28, 2.0f);
      drawList->AddCircle(ImVec2(screen.x, screen.y), radius + 5.0f,
                          IM_COL32(255, 110, 35, 90), 28, 1.0f);
      DrawWorldLabel(drawList, viewMatrix, screenW, screenH,
                     centroid + Vector3(0, 0, 10.0f), fireColor, "MOLLY", -14.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      continue;
    }
  }
}

} // namespace

void worldesp::Render() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  view_matrix_t &viewMatrix =
      *(view_matrix_t *)(clientBase + offsets::dwViewMatrix);
  ImVec2 displaySize = ImGui::GetIO().DisplaySize;
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();
  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  Vector3 localOrigin = {};
  if (localPawn) {
    uintptr_t localSceneNode =
        *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
    if (localSceneNode) {
      localOrigin =
          *(Vector3 *)(localSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
    }
  }

  RenderBombLocation(clientBase, drawList, viewMatrix, displaySize.x,
                     displaySize.y);
  RenderDroppedItems(entityList, drawList, viewMatrix, displaySize.x,
                     displaySize.y, localOrigin);
  RenderMolotovs(entityList, drawList, viewMatrix, displaySize.x,
                 displaySize.y);
}
