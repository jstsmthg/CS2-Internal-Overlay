#include "pch.h"

static Vector3 oldPunch = {};

void rcs::Run() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn)
    return;

  // Only compensate after first bullet (first shot has no recoil to track)
  int shotsFired = *(int *)(localPawn + schemas::C_CSPlayerPawn::m_iShotsFired);
  if (shotsFired < 2) {
    oldPunch = {};
    return;
  }

  // Read current punch angle from the AimPunchServices cache.
  // This contains the ACTUAL per-gun recoil pattern the engine is applying.
  uintptr_t aimPunchServices =
      *(uintptr_t *)(localPawn + schemas::C_CSPlayerPawn::m_pAimPunchServices);
  if (!aimPunchServices)
    return;

  // The punch cache is a CUtlVector<QAngle> at offset 0x70 in the service.
  // Layout: ptr to data (8 bytes), then size at +0x08, alloc at +0x0C.
  uintptr_t cacheData = *(uintptr_t *)(aimPunchServices + 0x70);
  int cacheSize = *(int *)(aimPunchServices + 0x70 + 0x08);

  if (cacheSize <= 0 || cacheSize > 0xFFFF || !cacheData)
    return;

  // Latest punch angle (the per-shot accumulation for THIS gun's pattern)
  Vector3 punch = *(Vector3 *)(cacheData + (cacheSize - 1) * sizeof(Vector3));

  // Delta since last frame — this IS the gun-specific recoil that happened
  float dx = punch.x - oldPunch.x;
  float dy = punch.y - oldPunch.y;
  oldPunch = punch;

  if (dx == 0.f && dy == 0.f)
    return;

  // Scale by 2.0 (Source 2 visual deviation = punch angle * 2)
  dx *= 2.0f;
  dy *= 2.0f;

  // Read current view angles and subtract the recoil delta directly.
  // This is frame-perfect and follows the exact spray pattern of whatever
  // gun the player is holding (AK-47, M4, etc.).
  Vector3 viewAngles = *(Vector3 *)(clientBase + offsets::dwViewAngles);

  viewAngles.x -= dx;
  viewAngles.y -= dy;
  viewAngles = ClampAngles(viewAngles);

  *(Vector3 *)(clientBase + offsets::dwViewAngles) = viewAngles;
}
