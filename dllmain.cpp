#include "pch.h"


// Global handle for FreeLibrary
static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_running{true};

// ============================================================================
// Simple file logger (non-blocking, unlike AllocConsole)
// ============================================================================
static FILE *g_logFile = nullptr;

void LogInit() { fopen_s(&g_logFile, "cs2_overlay.log", "w"); }

void Log(const char *fmt, ...) {
  if (!g_logFile)
    return;
  va_list args;
  va_start(args, fmt);
  vfprintf(g_logFile, fmt, args);
  va_end(args);
  fflush(g_logFile);
}

void LogClose() {
  if (g_logFile) {
    fclose(g_logFile);
    g_logFile = nullptr;
  }
}

// ============================================================================
// Main Thread — runs after DLL injection
// ============================================================================
DWORD WINAPI MainThread(LPVOID lpParam) {
  LogInit();
  Log("=== CS2 Overlay DLL Loaded ===\n");
  Log("[*] Module base: %p\n", g_hModule);

  // Load saved config
  config::Load();
  Log("[*] Config loaded\n");

  // Wait for the game to fully load
  Sleep(3000);

  HMODULE clientDll = GetModuleHandleA("client.dll");
  Log("[*] client.dll base: %p\n", clientDll);

  // Initialize hooks
  hooks::Init();

  // Keep alive until END key
  Log("[*] Main loop running. Press END to unload.\n");
  while (g_running) {
    if (GetAsyncKeyState(VK_END) & 1) {
      g_running = false;
    }
    Sleep(50);
  }

  // Save config before unloading
  config::Save();
  Log("[*] Config saved\n");

  // Cleanup
  Log("[*] Unloading...\n");

  hooks::Cleanup();
  Sleep(500);

  LogClose();

  FreeLibraryAndExitThread(g_hModule, 0);
  return 0;
}

// ============================================================================
// DLL Entry Point
// ============================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    g_hModule = hModule;
    DisableThreadLibraryCalls(hModule);
    {
      HANDLE hThread =
          CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
      if (hThread)
        CloseHandle(hThread);
    }
    break;

  case DLL_PROCESS_DETACH:
    g_running = false;

    break;
  }
  return TRUE;
}
