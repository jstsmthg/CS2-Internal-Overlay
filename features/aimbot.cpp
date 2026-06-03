#include "pch.h"

void aimbot::Run() {
  // Only active while the bound key is held
  if (!(GetAsyncKeyState(hooks::aimbotKey) & 0x8000))
    return;

  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  int localTeam = *(int *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);

  // Get local player controller for index lookup
  uintptr_t localController =
      *(uintptr_t *)(clientBase + offsets::dwLocalPlayerController);

  // Local eye position
  uintptr_t localSceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  if (!localSceneNode)
    return;

  Vector3 localOrigin =
      *(Vector3 *)(localSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);
  Vector3 eyePos = localOrigin + Vector3(0.f, 0.f, 64.f);

  // Entity list
  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  // Find local player's entity index (1-64) for spotted mask
  int localPlayerIndex = -1;
  if (hooks::aimbotVisCheck && localController) {
    for (int j = 1; j <= 64; j++) {
      uintptr_t le =
          *(uintptr_t *)(entityList + 0x10 + 8 * ((j & 0x7FFF) >> 9));
      if (!le)
        continue;
      uintptr_t ctrl = *(uintptr_t *)(le + 0x70 * (j & 0x1FF));
      if (ctrl == localController) {
        localPlayerIndex = j;
        break;
      }
    }
  }

  // Current view angles
  Vector3 currentAngles = *(Vector3 *)(clientBase + offsets::dwViewAngles);

  float bestAngleDist = hooks::aimbotFov;
  Vector3 bestAngle = {};
  bool found = false;

  for (int i = 1; i <= 64; i++) {
    uintptr_t listEntry =
        *(uintptr_t *)(entityList + 0x10 + 8 * ((i & 0x7FFF) >> 9));
    if (!listEntry)
      continue;

    uintptr_t controller = *(uintptr_t *)(listEntry + 0x70 * (i & 0x1FF));
    if (!controller)
      continue;

    uint32_t pawnHandle =
        *(uint32_t *)(controller + schemas::CCSPlayerController::m_hPlayerPawn);
    if (!pawnHandle)
      continue;

    uintptr_t pawnEntry =
        *(uintptr_t *)(entityList + 0x10 + 8 * ((pawnHandle & 0x7FFF) >> 9));
    if (!pawnEntry)
      continue;

    uintptr_t pawn =
        *(uintptr_t *)(pawnEntry + 0x70 * ((pawnHandle & 0x7FFF) & 0x1FF));
    if (!pawn || pawn == localPawn)
      continue;

    int health = *(int *)(pawn + schemas::C_BaseEntity::m_iHealth);
    int team = *(int *)(pawn + schemas::C_BaseEntity::m_iTeamNum);
    if (health <= 0 || (team == localTeam && !hooks::ignoreTeam))
      continue;

    // --- Visibility check via m_bSpottedByMask ---
    if (hooks::aimbotVisCheck && localPlayerIndex > 0) {
      // m_entitySpottedState is at C_CSPlayerPawn + 0x26E0
      // m_bSpottedByMask is at EntitySpottedState_t + 0xC (uint32[2])
      uintptr_t spottedMaskAddr =
          pawn + schemas::C_CSPlayerPawn::m_entitySpottedState + 0xC;
      uint32_t spottedMask[2];
      spottedMask[0] = *(uint32_t *)(spottedMaskAddr);
      spottedMask[1] = *(uint32_t *)(spottedMaskAddr + 4);

      // Player slot is (entityIndex - 1), 0-based
      int slot = localPlayerIndex - 1;
      int wordIndex = slot / 32;
      int bitIndex = slot % 32;
      if (!(spottedMask[wordIndex] & (1u << bitIndex)))
        continue; // Not visible to local player, skip
    }

    // --- Bone-based head position ---
    uintptr_t sceneNode =
        *(uintptr_t *)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode)
      continue;

    // CSkeletonInstance::m_modelState is at offset 0x160 from CGameSceneNode
    // Bone array pointer is at modelState + 0x80
    uintptr_t boneArray =
        *(uintptr_t *)(sceneNode + schemas::CSkeletonInstance::m_modelState +
                       0x80);

    Vector3 enemyHead;
    Vector3 enemyOrigin =
        *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);

    if (boneArray) {
      // Bone index 6 = neck, 7 = head in CS2, stride = 32 bytes
      enemyHead = GetBonePosition(boneArray, 7);


    } else {
      // Fallback: estimate head from origin
      enemyHead = enemyOrigin + Vector3(0.f, 0.f, 68.f);
    }

    // Validate head position (sanity check — bones can be zero during load)
    if (enemyHead.x == 0.f && enemyHead.y == 0.f && enemyHead.z == 0.f)
      continue;

    // Calculate angle to enemy head
    Vector3 targetAngle = CalcAngle(eyePos, enemyHead);

    // Angular distance from current view
    float dPitch = NormalizePitch(targetAngle.x - currentAngles.x);
    float dYaw = NormalizeYaw(targetAngle.y - currentAngles.y);
    float angleDist = std::sqrtf(dPitch * dPitch + dYaw * dYaw);

    if (angleDist < bestAngleDist) {
      bestAngleDist = angleDist;
      bestAngle = targetAngle;
      found = true;
    }
  }

  if (found) {
    float smooth = hooks::aimbotSmoothing;
    if (smooth < 1.0f)
      smooth = 1.0f;

    Vector3 delta;
    delta.x = NormalizePitch(bestAngle.x - currentAngles.x);
    delta.y = NormalizeYaw(bestAngle.y - currentAngles.y);
    delta.z = 0.f;

    Vector3 newAngles;
    newAngles.x = currentAngles.x + delta.x / smooth;
    newAngles.y = currentAngles.y + delta.y / smooth;
    newAngles.z = 0.f;
    newAngles = ClampAngles(newAngles);

    *(Vector3 *)(clientBase + offsets::dwViewAngles) = newAngles;
  }
}
