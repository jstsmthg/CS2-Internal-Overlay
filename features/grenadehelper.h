#pragma once
#include <vector>

// Forward declaring Vector3 is risky if we don't know the members, but the .cpp includes pch.h which defines Vector3.
// Wait, since this header will be included by menu.cpp and grenadehelper.cpp, we just need to ensure the project's vector struct is defined.
// The project uses `Vector3`, which is likely defined in math/vector.h or similar, and included via pch.h. 
// So we just declare the struct here. If menu.cpp includes pch.h before this, it's fine.

struct GrenadeSpot {
  char name[64];
  int type;
  Vector3 origin;
  Vector3 angles;
};

namespace grenadehelper {
void RequestSaveCurrentSpot();
const char *CurrentMapName();
int LoadedSpotCount();
void RefreshCurrentMap();
void Render();

std::vector<GrenadeSpot>& GetSpots();
void DeleteSpot(int index);
void SaveAllSpots();
}
