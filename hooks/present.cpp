#include "pch.h"

// Forward declare ImGui WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

// External logger (defined in dllmain.cpp)
extern void Log(const char *fmt, ...);

// ============================================================================
// Typedefs
// ============================================================================
using PresentFn = HRESULT(__stdcall *)(IDXGISwapChain *, UINT, UINT);

// ============================================================================
// Globals
// ============================================================================
static PresentFn oPresent = nullptr;
static WNDPROC oWndProc = nullptr;
static HWND gameWindow = nullptr;
static ID3D11Device *pDevice = nullptr;
static ID3D11DeviceContext *pContext = nullptr;
static ID3D11RenderTargetView *pRenderTargetView = nullptr;
static bool initialized = false;

// ============================================================================
// Helper: tear down ImGui + DX11 state so it can be re-initialized
// ============================================================================
static void ResetImGuiState() {
  if (initialized) {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
  }
  if (oWndProc && gameWindow) {
    SetWindowLongPtrA(gameWindow, GWLP_WNDPROC, (LONG_PTR)oWndProc);
    oWndProc = nullptr;
  }
  if (pRenderTargetView) {
    pRenderTargetView->Release();
    pRenderTargetView = nullptr;
  }
  if (pContext) {
    pContext->Release();
    pContext = nullptr;
  }
  if (pDevice) {
    pDevice->Release();
    pDevice = nullptr;
  }
  gameWindow = nullptr;
  initialized = false;
}

// ============================================================================
// VK key name helper
// ============================================================================
static const char *GetVKKeyName(int vk) {
  switch (vk) {
  case VK_LBUTTON:  return "Mouse1";
  case VK_RBUTTON:  return "Mouse2";
  case VK_MBUTTON:  return "Mouse3";
  case VK_XBUTTON1: return "Mouse4";
  case VK_XBUTTON2: return "Mouse5";
  case VK_LSHIFT:   return "LShift";
  case VK_RSHIFT:   return "RShift";
  case VK_LCONTROL: return "LCtrl";
  case VK_RCONTROL: return "RCtrl";
  case VK_LMENU:    return "LAlt";
  case VK_RMENU:    return "RAlt";
  case VK_CAPITAL:  return "CapsLock";
  case VK_TAB:      return "Tab";
  case VK_SPACE:    return "Space";
  case VK_BACK:     return "Backspace";
  case VK_ESCAPE:   return "Escape";
  default: {
    // A-Z, 0-9
    if (vk >= 0x30 && vk <= 0x39) { static char buf[2]; buf[0] = (char)vk; buf[1] = 0; return buf; }
    if (vk >= 0x41 && vk <= 0x5A) { static char buf[2]; buf[0] = (char)vk; buf[1] = 0; return buf; }
    // F1-F12
    if (vk >= VK_F1 && vk <= VK_F12) { static char buf[4]; snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1); return buf; }
    static char buf[8]; snprintf(buf, sizeof(buf), "0x%02X", vk); return buf;
  }
  }
}

// ============================================================================
// WndProc hook
// ============================================================================
static LRESULT CALLBACK hWndProc(HWND hWnd, UINT msg, WPARAM wParam,
                                 LPARAM lParam) {
  // Key binding capture — intercept any key/mouse press while waiting
  if (hooks::waitingForAimbotKey) {
    if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam != VK_INSERT) {
      hooks::aimbotKey = (int)wParam;
      hooks::waitingForAimbotKey = false;
      return 0;
    }
    if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
        msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN) {
      int btn = 0;
      if (msg == WM_LBUTTONDOWN) btn = VK_LBUTTON;
      else if (msg == WM_RBUTTONDOWN) btn = VK_RBUTTON;
      else if (msg == WM_MBUTTONDOWN) btn = VK_MBUTTON;
      else if (msg == WM_XBUTTONDOWN) btn = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
      hooks::aimbotKey = btn;
      hooks::waitingForAimbotKey = false;
      return 0;
    }
  }

  if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
    hooks::showMenu = !hooks::showMenu;
    return 0;
  }

  if (hooks::showMenu) {
    LPARAM passLParam = lParam;
    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
      int x = (short)LOWORD(lParam);
      int y = (short)HIWORD(lParam);
      x = (int)(x * hooks::scaleX);
      y = (int)(y * hooks::scaleY);
      passLParam = MAKELPARAM(x, y);
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, passLParam))
      return 0;
    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)
      return 0;
  }

  return CallWindowProcA(oWndProc, hWnd, msg, wParam, lParam);
}

// ============================================================================
// Hooked Present — survives map changes / device recreation
// ============================================================================
static HRESULT __stdcall hPresent(IDXGISwapChain *pSwapChain, UINT SyncInterval,
                                  UINT Flags) {
  // ---- Device change detection ----
  // If the game recreates its device (map change), our pointers go stale.
  // Detect this by querying the current device and comparing.
  if (initialized) {
    ID3D11Device *currentDevice = nullptr;
    pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&currentDevice);
    if (currentDevice != pDevice) {
      Log("[!] Device changed — reinitializing ImGui\n");
      ResetImGuiState();
      // initialized is now false, will re-init below
    }
    if (currentDevice)
      currentDevice->Release();
  }

  // ---- First-time or re-initialization ----
  if (!initialized) {
    if (FAILED(
            pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice))) {
      return oPresent(pSwapChain, SyncInterval, Flags);
    }
    pDevice->GetImmediateContext(&pContext);

    DXGI_SWAP_CHAIN_DESC desc;
    pSwapChain->GetDesc(&desc);
    gameWindow = desc.OutputWindow;

    oWndProc = (WNDPROC)SetWindowLongPtrA(gameWindow, GWLP_WNDPROC,
                                          (LONG_PTR)hWndProc);

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.12f, 0.94f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.32f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.24f, 0.54f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

    ImGui_ImplWin32_Init(gameWindow);
    ImGui_ImplDX11_Init(pDevice, pContext);

    initialized = true;
    Log("[+] ImGui initialized (device: %p)\n", pDevice);
  }

  // ---- Recreate render target every frame (safe for resize/map change) ----
  if (pRenderTargetView) {
    pRenderTargetView->Release();
    pRenderTargetView = nullptr;
  }

  ID3D11Texture2D *backBuffer = nullptr;
  pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backBuffer);
  if (!backBuffer) {
    return oPresent(pSwapChain, SyncInterval, Flags);
  }
  pDevice->CreateRenderTargetView(backBuffer, nullptr, &pRenderTargetView);
  backBuffer->Release();

  if (!pRenderTargetView) {
    return oPresent(pSwapChain, SyncInterval, Flags);
  }

  // ---- Begin ImGui frame ----
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();

  // Override ImGui DisplaySize and MousePos to handle Stretched Aspect Ratios correctly
  DXGI_SWAP_CHAIN_DESC sd;
  if (SUCCEEDED(pSwapChain->GetDesc(&sd))) {
    ImGuiIO &io = ImGui::GetIO();
    RECT rect;
    if (GetClientRect(gameWindow, &rect)) {
      float clientW = (float)(rect.right - rect.left);
      float clientH = (float)(rect.bottom - rect.top);
      float swapW = (float)sd.BufferDesc.Width;
      float swapH = (float)sd.BufferDesc.Height;

      io.DisplaySize = ImVec2(swapW, swapH);

      if (clientW > 0.0f && clientH > 0.0f) {
        hooks::scaleX = swapW / clientW;
        hooks::scaleY = swapH / clientH;

        POINT pos;
        if (::GetCursorPos(&pos) && ::ScreenToClient(gameWindow, &pos)) {
          io.AddMousePosEvent((float)pos.x * hooks::scaleX,
                              (float)pos.y * hooks::scaleY);
        }
      }
    }
  }

  ImGui::NewFrame();

  // ---- Menu ----
  if (hooks::showMenu) {
    ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("CS2 Overlay", &hooks::showMenu, ImGuiWindowFlags_NoCollapse);

    // == Combat ==
    ImGui::TextColored(ImVec4(0.98f, 0.4f, 0.4f, 1.0f), "Combat");
    ImGui::Separator();
    ImGui::Checkbox("Deathmatch Mode (Target Team)", &hooks::ignoreTeam);
    {
      char aimbotLabel[64];
      snprintf(aimbotLabel, sizeof(aimbotLabel), "Aimbot [%s]", GetVKKeyName(hooks::aimbotKey));
      ImGui::Checkbox(aimbotLabel, &hooks::aimbotEnabled);
    }
    if (hooks::aimbotEnabled) {
      ImGui::SliderFloat("Smoothing", &hooks::aimbotSmoothing, 1.0f, 20.0f);
      ImGui::SliderFloat("FOV", &hooks::aimbotFov, 1.0f, 90.0f);
      ImGui::Checkbox("Visibility Check", &hooks::aimbotVisCheck);
      if (hooks::waitingForAimbotKey) {
        ImGui::Button("Press any key...");
      } else {
        if (ImGui::Button("Bind Key")) {
          hooks::waitingForAimbotKey = true;
        }
      }
    }
    ImGui::Checkbox("Triggerbot", &hooks::triggerbotEnabled);
    if (hooks::triggerbotEnabled) {
      ImGui::SliderInt("Delay (ms)", &hooks::triggerbotDelay, 0, 200);
    }
    ImGui::Checkbox("RCS (Recoil Control)", &hooks::rcsEnabled);
    ImGui::Spacing();

    // == Visuals ==
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.98f, 1.0f), "Visuals");
    ImGui::Separator();
    ImGui::Checkbox("ESP", &hooks::espEnabled);
    if (hooks::espEnabled) {
      ImGui::Combo("ESP Style", &hooks::espStyle, "2D Boxes\0" "3D Glow (Silhouettes)\0");
    }
    ImGui::Checkbox("Radar Hack", &hooks::radarEnabled);
    ImGui::Checkbox("Bomb Timer", &hooks::bombTimerEnabled);



    ImGui::Spacing();

    // == Config ==
    ImGui::TextColored(ImVec4(0.6f, 0.98f, 0.6f, 1.0f), "Config");
    ImGui::Separator();
    if (ImGui::Button("Save Config"))
      config::Save();
    ImGui::SameLine();
    if (ImGui::Button("Load Config"))
      config::Load();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("[INSERT] toggle menu | [END] eject");
    ImGui::End();
  }

  // ---- Features (SEH protected — game memory may be invalid during map load)
  // ----
  if (hooks::espEnabled) {
    __try {
      esp::Render();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  
  if (hooks::aimbotEnabled) {
    __try {
      aimbot::Run();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  
  if (hooks::triggerbotEnabled) {
    __try {
      triggerbot::Run();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  
  if (hooks::rcsEnabled) {
    __try {
      rcs::Run();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  
  if (hooks::bombTimerEnabled) {
    __try {
      bombtimer::Render();
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  


  // ---- Render ----
  ImGui::Render();
  pContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

  return oPresent(pSwapChain, SyncInterval, Flags);
}

// ============================================================================
// Get Present address via dummy swap chain + temporary hidden window
// ============================================================================
static void *GetPresentAddress() {
  WNDCLASSEXA wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.lpszClassName = "DummyDX11Window";
  RegisterClassExA(&wc);

  HWND dummyHwnd =
      CreateWindowExA(0, wc.lpszClassName, "Dummy", WS_OVERLAPPEDWINDOW, 0, 0,
                      100, 100, nullptr, nullptr, wc.hInstance, nullptr);

  if (!dummyHwnd) {
    Log("[-] Failed to create dummy window\n");
    return nullptr;
  }

  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 1;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = dummyHwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  IDXGISwapChain *dummySwapChain = nullptr;
  ID3D11Device *dummyDevice = nullptr;
  ID3D11DeviceContext *dummyContext = nullptr;

  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
      D3D11_SDK_VERSION, &sd, &dummySwapChain, &dummyDevice, &featureLevel,
      &dummyContext);

  if (FAILED(hr)) {
    Log("[-] D3D11CreateDeviceAndSwapChain FAILED: 0x%08lX\n", hr);
    DestroyWindow(dummyHwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return nullptr;
  }

  void **vtable = *reinterpret_cast<void ***>(dummySwapChain);
  void *presentAddr = vtable[8];
  Log("[+] Present vtable[8]: %p\n", presentAddr);

  dummySwapChain->Release();
  dummyDevice->Release();
  dummyContext->Release();
  DestroyWindow(dummyHwnd);
  UnregisterClassA(wc.lpszClassName, wc.hInstance);

  return presentAddr;
}

// ============================================================================
// Public API
// ============================================================================
void hooks::Init() {
  Log("[*] hooks::Init()\n");

  MH_STATUS s = MH_Initialize();
  Log("[*] MH_Initialize: %s\n", MH_StatusToString(s));

  void *target = GetPresentAddress();
  if (!target) {
    Log("[-] No Present address\n");
    return;
  }

  s = MH_CreateHook(target, &hPresent, reinterpret_cast<void **>(&oPresent));
  Log("[*] MH_CreateHook: %s\n", MH_StatusToString(s));

  s = MH_EnableHook(target);
  Log("[*] MH_EnableHook: %s\n", MH_StatusToString(s));
}

void hooks::Cleanup() {
  Log("[*] hooks::Cleanup()\n");

  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();

  ResetImGuiState();
}
