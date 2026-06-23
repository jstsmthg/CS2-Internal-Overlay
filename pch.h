// pch.h: Precompiled header for the CS2 DX11 overlay plugin

#ifndef PCH_H
#define PCH_H

// Windows
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

// Standard library
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


// MinHook
#include "MinHook.h"

// ImGui
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// SDK
#include "sdk/offsets.h"
#include "sdk/structs.h"

// Project headers
#include "features/aimbot.h"
#include "features/bombtimer.h"
#include "features/config.h"
#include "features/esp.h"
#include "features/grenadehelper.h"
#include "features/menu.h"
#include "features/misc.h"
#include "features/radar.h"
#include "features/rcs.h"
#include "features/transcript.h"
#include "features/triggerbot.h"
#include "features/worldesp.h"
#include "hooks/present.h"


#endif // PCH_H
