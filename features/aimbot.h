#pragma once

namespace aimbot {
void Run();
// Returns: 0=Unknown, 1=Rifle, 2=SMG, 3=Pistol, 4=Sniper, 5=Shotgun, 6=Melee
int GetWeaponCategory(uintptr_t localPawn, uintptr_t entityList);
}
