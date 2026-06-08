#include "pch.h"
#include <chrono>

// Delta-time tracking for frame-rate-independent smoothing
static auto s_lastFrame = std::chrono::steady_clock::now();

// ============================================================================
// Hitbox → Bone index mapping
// ============================================================================
static int HitboxToBone(int hitbox) {
  switch (hitbox) {
  case 0: return 7;  // Head
  case 1: return 5;  // Neck
  case 2: return 4;  // Chest (Spine2)
  case 3: return 0;  // Pelvis (Root)
  case 4: return 23; // Legs (left calf, center mass)
  default: return 7; // Fallback to head
  }
}

// All bones to check for closest-hitbox mode
static const int g_allBones[] = { 7, 5, 4, 0, 23 };
static const int g_allBonesCount = 5;

static bool IsZeroVector(const Vector3 &v) {
  return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
}

static Vector3 AdjustHitboxPoint(int hitbox, const Vector3 &point,
                                 const Vector3 &headPoint,
                                 const Vector3 &neckPoint) {
  Vector3 adjusted = point;
  switch (hitbox) {
  case 0:
    // Bone 7 is the exact head center, no Z offset needed.
    break;
  case 1:
    if (!IsZeroVector(headPoint) && !IsZeroVector(neckPoint))
      adjusted = neckPoint + (headPoint - neckPoint) * 0.35f;
    break;
  case 2:
    adjusted.z += 1.5f;
    break;
  default:
    break;
  }
  return adjusted;
}

static Vector3 GetConfiguredHitboxPosition(uintptr_t boneArray, int hitbox,
                                           const Vector3 &enemyOrigin) {
  Vector3 headPoint = {};
  Vector3 neckPoint = {};
  if (boneArray) {
    headPoint = GetBonePosition(boneArray, 6);
    neckPoint = GetBonePosition(boneArray, 5);
  }

  Vector3 basePos;
  if (boneArray) {
    basePos = GetBonePosition(boneArray, HitboxToBone(hitbox));
  } else {
    float height = 68.f;
    switch (hitbox) {
    case 0: height = 68.f; break;
    case 1: height = 62.f; break;
    case 2: height = 48.f; break;
    case 3: height = 0.f;  break;
    case 4: height = 20.f; break;
    }
    basePos = enemyOrigin + Vector3(0.f, 0.f, height);
    if (hitbox == 0)
      basePos.z += 4.5f;
  }

  return AdjustHitboxPoint(hitbox, basePos, headPoint, neckPoint);
}

static Vector3 PredictTargetPosition(const Vector3 &basePos,
                                     const Vector3 &enemyVel, float dt) {
  // The previous dt*3 lead was over-predicting lateral strafes and pulling the
  // aim noticeably off to the side. Keep prediction modest and horizontal-only.
  float speed2D = enemyVel.Length2D();
  if (speed2D < 75.0f)
    return basePos;

  float predTime = dt * 1.25f;
  if (predTime > 0.020f)
    predTime = 0.020f;
  if (predTime < 0.0f)
    predTime = 0.0f;

  Vector3 horizVel(enemyVel.x, enemyVel.y, 0.0f);
  return basePos + horizVel * predTime;
}

// ============================================================================
// Weapon Category Detection
// Returns: 0=Unknown, 1=Rifle, 2=SMG, 3=Pistol, 4=Sniper, 5=Shotgun, 6=Melee
// ============================================================================
int aimbot::GetWeaponCategory(uintptr_t localPawn, uintptr_t entityList) {
  __try {
    // pawn + 0x11E0 → CPlayer_WeaponServices*
    uintptr_t weaponServices =
        *(uintptr_t *)(localPawn +
                       schemas::C_BasePlayerPawn::m_pWeaponServices);
    if (!weaponServices)
      return 0;

    // weaponServices + 0x60 → CHandle<C_BasePlayerWeapon> (active weapon handle)
    uint32_t activeHandle =
        *(uint32_t *)(weaponServices +
                      schemas::CPlayer_WeaponServices::m_hActiveWeapon);
    if (!activeHandle || activeHandle == 0xFFFFFFFF)
      return 0;

    // Resolve handle via entity list
    uintptr_t weaponEntry =
        *(uintptr_t *)(entityList + 0x10 +
                       8 * ((activeHandle & 0x7FFF) >> 9));
    if (!weaponEntry)
      return 0;

    uintptr_t weaponEntity =
        *(uintptr_t *)(weaponEntry +
                       0x70 * ((activeHandle & 0x7FFF) & 0x1FF));
    if (!weaponEntity)
      return 0;

    // Read item definition index via schema chain:
    // C_EconEntity::m_AttributeManager (0x1180) -> C_AttributeContainer::m_Item (0x50) ->
    // C_EconItemView::m_iItemDefinitionIndex (0x1BA)
    uint16_t itemDefIndex =
        *(uint16_t *)(weaponEntity +
                      schemas::C_EconEntity::m_AttributeManager +
                      schemas::C_AttributeContainer::m_Item +
                      schemas::C_EconItemView::m_iItemDefinitionIndex);

    // Map weapon definition ID to category
    switch (itemDefIndex) {
    // Rifles
    case 7:  // AK-47
    case 16: // M4A4
    case 60: // M4A1-S
    case 13: // Galil
    case 10: // Famas
    case 8:  // AUG
    case 39: // SG 553
      return 1;

    // SMGs
    case 34: // MP9
    case 17: // MAC-10
    case 33: // MP7
    case 24: // UMP-45
    case 19: // P90
    case 26: // PP-Bizon
    case 23: // MP5-SD
      return 2;

    // Pistols
    case 4:  // Glock
    case 61: // USP-S
    case 32: // P2000
    case 36: // P250
    case 3:  // Five-SeveN
    case 63: // CZ75-Auto
    case 1:  // Desert Eagle
    case 64: // R8 Revolver
    case 2:  // Dual Berettas
    case 30: // Tec-9
      return 3;

    // Snipers
    case 9:  // AWP
    case 40: // SSG 08 (Scout)
    case 38: // SCAR-20
    case 11: // G3SG1
      return 4;

    // Shotguns / MGs
    case 35: // Nova
    case 25: // XM1014
    case 27: // MAG-7
    case 29: // Sawed-Off
    case 14: // M249
    case 28: // Negev
      return 5;

    // Melee
    case 42: // Knife (CT)
    case 59: // Knife (T)
    case 31: // Zeus x27
      return 6;

    default:
      return 0;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

void aimbot::Run() {
  // Only active while the bound key is held
  if (!(GetAsyncKeyState(hooks::aimbotKey) & 0x8000))
    return;

  // ---- Delta-time calculation ----
  auto now = std::chrono::steady_clock::now();
  float dt = std::chrono::duration<float>(now - s_lastFrame).count();
  s_lastFrame = now;
  // Clamp dt to avoid huge jumps on first frame or hitches
  if (dt <= 0.f || dt > 0.1f)
    dt = 0.016f; // fallback ~60fps

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

  // Local eye position using actual m_vecViewOffset (handles crouching)
  uintptr_t localSceneNode =
      *(uintptr_t *)(localPawn + schemas::C_BaseEntity::m_pGameSceneNode);
  if (!localSceneNode)
    return;

  Vector3 localOrigin =
      *(Vector3 *)(localSceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);

  // Local eye position using actual m_vecViewOffset (handles crouching)
  // Read the full view offset, as using only Z or adding padding caused twitching or vertical offset.
  Vector3 viewOffset = *(Vector3 *)(localPawn + schemas::C_BaseModelEntity::m_vecViewOffset);
  Vector3 eyePos = localOrigin + viewOffset;

  // Entity list
  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList)
    return;

  // Weapon category filter: skip aimbot if weapon doesn't match selected category
  if (hooks::aimbotWeaponCat > 0) {
    int weaponCat = aimbot::GetWeaponCategory(localPawn, entityList);
    if (weaponCat != hooks::aimbotWeaponCat)
      return;
  }

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
      uintptr_t spottedMaskAddr =
          pawn + schemas::C_CSPlayerPawn::m_entitySpottedState + 0xC;
      uint32_t spottedMask[2];
      spottedMask[0] = *(uint32_t *)(spottedMaskAddr);
      spottedMask[1] = *(uint32_t *)(spottedMaskAddr + 4);

      int slot = localPlayerIndex - 1;
      int wordIndex = slot / 32;
      int bitIndex = slot % 32;
      if (!(spottedMask[wordIndex] & (1u << bitIndex)))
        continue; // Not visible to local player, skip
    }

    // --- Bone-based target position ---
    uintptr_t sceneNode =
        *(uintptr_t *)(pawn + schemas::C_BaseEntity::m_pGameSceneNode);
    if (!sceneNode)
      continue;

    uintptr_t boneArray =
        *(uintptr_t *)(sceneNode + schemas::CSkeletonInstance::m_modelState +
                       0x80);

    Vector3 enemyOrigin =
        *(Vector3 *)(sceneNode + schemas::CGameSceneNode::m_vecAbsOrigin);

    // --- Velocity-based prediction data ---
    Vector3 enemyVel = *(Vector3 *)(pawn + schemas::C_BaseEntity::m_vecAbsVelocity);

    if (hooks::aimbotClosestHitbox) {
      // --- Closest hitbox mode: check all bones, pick smallest angular distance ---
      for (int bi = 0; bi < g_allBonesCount; bi++) {
        int boneIdx = g_allBones[bi];
        Vector3 bonePos;

        if (boneArray) {
          bonePos = GetConfiguredHitboxPosition(boneArray, bi, enemyOrigin);
        } else {
          float heights[] = { 72.5f, 62.f, 49.5f, 0.f, 20.f };
          bonePos = enemyOrigin + Vector3(0.f, 0.f, heights[bi]);
        }

        if (bonePos.x == 0.f && bonePos.y == 0.f && bonePos.z == 0.f)
          continue;

        Vector3 predictedPos = PredictTargetPosition(bonePos, enemyVel, dt);
        Vector3 targetAngle = CalcAngle(eyePos, predictedPos);

        float dPitch = NormalizePitch(targetAngle.x - currentAngles.x);
        float dYaw = NormalizeYaw(targetAngle.y - currentAngles.y);
        float angleDist = std::sqrtf(dPitch * dPitch + dYaw * dYaw);

        if (angleDist < bestAngleDist) {
          bestAngleDist = angleDist;
          bestAngle = targetAngle;
          found = true;
        }
      }
    } else {
      // --- Single hitbox mode: use selected bone ---
      Vector3 targetPos =
          GetConfiguredHitboxPosition(boneArray, hooks::aimbotHitbox, enemyOrigin);

      if (targetPos.x == 0.f && targetPos.y == 0.f && targetPos.z == 0.f)
        continue;

      Vector3 predictedPos = PredictTargetPosition(targetPos, enemyVel, dt);
      Vector3 targetAngle = CalcAngle(eyePos, predictedPos);

      float dPitch = NormalizePitch(targetAngle.x - currentAngles.x);
      float dYaw = NormalizeYaw(targetAngle.y - currentAngles.y);
      float angleDist = std::sqrtf(dPitch * dPitch + dYaw * dYaw);

      if (angleDist < bestAngleDist) {
        bestAngleDist = angleDist;
        bestAngle = targetAngle;
        found = true;
      }
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
    if (smooth <= 1.01f) {
      // Instant snap — no smoothing at all
      newAngles = bestAngle;
    } else if (hooks::aimbotLinearSmooth) {
      // Linear smoothing: newAngles = currentAngles + delta * (1/smooth)
      float factor = 1.0f / smooth;
      if (factor > 1.0f)
        factor = 1.0f;

      newAngles.x = currentAngles.x + delta.x * factor;
      newAngles.y = currentAngles.y + delta.y * factor;
    } else {
      // Exponential smoothing: frame-rate independent
      // smooth=2 → very fast tracking, smooth=20 → gentle pull
      float speed = 500.0f / smooth;
      float factor = 1.0f - std::expf(-speed * dt);
      if (factor > 1.0f)
        factor = 1.0f;

      newAngles.x = currentAngles.x + delta.x * factor;
      newAngles.y = currentAngles.y + delta.y * factor;
    }
    newAngles.z = 0.f;
    newAngles = ClampAngles(newAngles);

    *(Vector3 *)(clientBase + offsets::dwViewAngles) = newAngles;
  }
}
