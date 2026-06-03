#include "pch.h"

// State machine for tap-fire triggerbot
enum class TBState { Idle, Delaying, Firing, Cooldown };

static TBState       state           = TBState::Idle;
static ULONGLONG     stateStartTime  = 0;
static const int     FIRE_HOLD_MS    = 40;   // how long to hold +attack (one tap)
static const int     COOLDOWN_MS     = 80;   // minimum gap between shots

// Helper: release the attack button
static void ReleaseAttack(uintptr_t clientBase) {
  *(int *)(clientBase + buttons::attack) = 256;
}

void triggerbot::Run() {
  uintptr_t clientBase = (uintptr_t)GetModuleHandleA("client.dll");
  if (!clientBase)
    return;

  uintptr_t localPawn = *(uintptr_t *)(clientBase + offsets::dwLocalPlayerPawn);
  if (!localPawn) {
    ReleaseAttack(clientBase);
    state = TBState::Idle;
    return;
  }

  int localTeam = *(int *)(localPawn + schemas::C_BaseEntity::m_iTeamNum);

  // Read crosshair entity index
  int crosshairEntIndex =
      *(int *)(localPawn + schemas::C_CSPlayerPawn::m_iIDEntIndex);

  // ---- No valid target under crosshair ----
  if (crosshairEntIndex <= 0) {
    // If we were mid-fire, make sure we release the button
    if (state == TBState::Firing)
      ReleaseAttack(clientBase);
    state = TBState::Idle;
    return;
  }

  // Resolve the entity under crosshair
  uintptr_t entityList = *(uintptr_t *)(clientBase + offsets::dwEntityList);
  if (!entityList) {
    if (state == TBState::Firing)
      ReleaseAttack(clientBase);
    state = TBState::Idle;
    return;
  }

  uintptr_t listEntry = *(uintptr_t *)(entityList + 0x10 +
                                       8 * ((crosshairEntIndex & 0x7FFF) >> 9));
  if (!listEntry) {
    if (state == TBState::Firing)
      ReleaseAttack(clientBase);
    state = TBState::Idle;
    return;
  }

  uintptr_t targetEntity =
      *(uintptr_t *)(listEntry + 0x70 * ((crosshairEntIndex & 0x7FFF) & 0x1FF));
  if (!targetEntity) {
    if (state == TBState::Firing)
      ReleaseAttack(clientBase);
    state = TBState::Idle;
    return;
  }

  int targetTeam = *(int *)(targetEntity + schemas::C_BaseEntity::m_iTeamNum);
  int targetHealth = *(int *)(targetEntity + schemas::C_BaseEntity::m_iHealth);

  if (targetHealth <= 0 || (targetTeam == localTeam && !hooks::ignoreTeam)) {
    if (state == TBState::Firing)
      ReleaseAttack(clientBase);
    state = TBState::Idle;
    return;
  }

  // ---- Valid enemy in crosshair — run the state machine ----
  ULONGLONG now = GetTickCount64();

  switch (state) {

  case TBState::Idle:
    // Start the pre-fire delay (or fire immediately if delay == 0)
    state = TBState::Delaying;
    stateStartTime = now;
    // fall through so a zero-delay fires on this same frame
    [[fallthrough]];

  case TBState::Delaying:
    if (hooks::triggerbotDelay > 0 &&
        (now - stateStartTime) < (ULONGLONG)hooks::triggerbotDelay) {
      return; // still waiting
    }
    // Delay elapsed — press attack
    *(int *)(clientBase + buttons::attack) = 65537;
    state = TBState::Firing;
    stateStartTime = now;
    break;

  case TBState::Firing:
    // Hold the button for FIRE_HOLD_MS then release
    if ((now - stateStartTime) >= FIRE_HOLD_MS) {
      ReleaseAttack(clientBase);
      state = TBState::Cooldown;
      stateStartTime = now;
    }
    break;

  case TBState::Cooldown:
    // Wait between shots so we don't full-auto dump the magazine
    if ((now - stateStartTime) >= COOLDOWN_MS) {
      state = TBState::Idle;
    }
    break;
  }
}
