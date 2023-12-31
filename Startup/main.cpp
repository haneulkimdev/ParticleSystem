#include <d3d11.h>
#include <tchar.h>
#include <wrl/client.h>

#include "../MyEngine/MyEngineAPI.h"
#include "SimpleMath.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "spdlog/spdlog.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX::SimpleMath;

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

auto g_apiLogger = spdlog::default_logger();

// Main code
int main(int, char**) {
  g_apiLogger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  my::InitEngine(g_apiLogger);

  static ImVec2 renderTargetSize = ImVec2(512, 512);
  my::SetRenderTargetSize(static_cast<int>(renderTargetSize.x),
                          static_cast<int>(renderTargetSize.y));

  // Create application window
  // ImGui_ImplWin32_EnableDpiAwareness();
  WNDCLASSEXW wc = {sizeof(wc),
                    CS_CLASSDC,
                    WndProc,
                    0L,
                    0L,
                    GetModuleHandle(nullptr),
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    L"ImGui Example",
                    nullptr};
  ::RegisterClassExW(&wc);
  HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example",
                              WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr,
                              nullptr, wc.hInstance, nullptr);

  // Initialize Direct3D
  if (!CreateDeviceD3D(hwnd)) {
    CleanupDeviceD3D();
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 1;
  }

  // Show the window
  ::ShowWindow(hwnd, SW_SHOWDEFAULT);
  ::UpdateWindow(hwnd);
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

  // Our state
  bool show_demo_window = false;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  int mouseButton = -1;
  Vector2 lastMousePos = Vector2(-1, -1);

  // Main loop
  bool done = false;
  while (!done) {
    // Poll and handle messages (inputs, window resize, etc.)
    // See the WndProc() function below for our to dispatch events to the Win32
    // backend.
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;

    // Handle window resize (we don't resize directly in the WM_SIZE handler)
    if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight,
                                  DXGI_FORMAT_UNKNOWN, 0);
      g_ResizeWidth = g_ResizeHeight = 0;
      CreateRenderTarget();
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
    // ImGui!).
    if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair
    // to create a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!"
                                      // and append into it.

      ImGui::Text("This is some useful text.");  // Display some text (you can
                                                 // use a format strings too)
      ImGui::Checkbox(
          "Demo Window",
          &show_demo_window);  // Edit bools storing our window open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat(
          "float", &f, 0.0f,
          1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3(
          "clear color",
          (float*)&clear_color);  // Edit 3 floats representing a color

      if (ImGui::Button(
              "Button"))  // Buttons return true when clicked (most widgets
                          // return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window) {
      ImGui::Begin(
          "Another Window",
          &show_another_window);  // Pass a pointer to our bool variable (the
                                  // window will have a closing button that will
                                  // clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me")) show_another_window = false;
      ImGui::End();
    }

    {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
      ImGui::Begin("DirectX11 Texture Test");

      static ImVec2 lastWindowSize = ImVec2(0, 0);
      ImVec2 windowSize = ImGui::GetWindowSize();
      if (lastWindowSize.x != windowSize.x ||
          lastWindowSize.y != windowSize.y) {
        if (windowSize.x > 32 && windowSize.y > 32) {
          renderTargetSize = ImGui::GetContentRegionAvail();
        }

        my::SetRenderTargetSize(static_cast<int>(renderTargetSize.x),
                                static_cast<int>(renderTargetSize.y));

        lastWindowSize = windowSize;
      }

      ImVec2 cursorPos = ImGui::GetCursorPos();
      ImGui::InvisibleButton(
          "Invisible Button", renderTargetSize,
          ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
      ImGui::SetItemAllowOverlap();

      my::Update(ImGui::GetIO().DeltaTime);
      my::DoTest();

      ID3D11ShaderResourceView* textureView = nullptr;
      int w, h;
      my::GetDX11SharedRenderTarget(g_pd3dDevice, &textureView, w, h);

      ImGui::SetCursorPos(cursorPos);
      ImGui::Image((void*)textureView, renderTargetSize);
      ImGui::End();
      ImGui::PopStyleVar();
    }

    {
      const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
      ImGui::SetNextWindowPos(
          ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20),
          ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

      ImGui::Begin("MyEngine Settings");

      if (ImGui::CollapsingHeader("Emitter")) {
        my::EmittedParticleSystem* emitter;
        my::GetEmitter(&emitter);

        int maxCount = emitter->GetMaxParticleCount();
        ImGui::SliderInt("Max Count", (int*)&maxCount, 1, 1e6);
        emitter->SetMaxParticleCount(maxCount);

        ImGui::SliderFloat("Emit", &emitter->count, 0.0f, 10000.0f);
        ImGui::SliderFloat("Size", &emitter->size, 0.01f, 10.0f);
        ImGui::SliderFloat("Rotation", &emitter->rotation, 0.0f, 1.0f);
        ImGui::SliderFloat("Normal", &emitter->normal_factor, 0.0f, 100.0f);
        ImGui::SliderFloat("Scale X", &emitter->scaleX, 0.0f, 100.0f);
        ImGui::SliderFloat("Scale Y", &emitter->scaleY, 0.0f, 100.0f);
        ImGui::SliderFloat("Life Span", &emitter->life, 0.0f, 100.0f);
        ImGui::SliderFloat("Life Randomness", &emitter->random_life, 0.0f,
                           2.0f);
        ImGui::SliderFloat("Randomness", &emitter->random_factor, 0.0f, 10.0f);
        ImGui::SliderFloat("Color Randomness", &emitter->random_color, 0.0f,
                           2.0f);
        ImGui::SliderFloat("Motion Blur", &emitter->motionBlurAmount, 0.0f,
                           1.0f);
        ImGui::SliderFloat("Mass", &emitter->mass, 0.1f, 100.0f);
        ImGui::SliderFloat("Timestep", &emitter->FIXED_TIMESTEP, -1.0f, 1.0f);
        ImGui::SliderFloat("Drag", &emitter->drag, 0.0f, 1.0f);
        ImGui::SliderFloat("Restitution", &emitter->restitution, 0.0f, 1.0f);

        float velocity[3] = {emitter->velocity.x, emitter->velocity.y,
                             emitter->velocity.z};
        ImGui::InputFloat3("Velocity", velocity);

        float gravity[3] = {emitter->gravity.x, emitter->gravity.y,
                            emitter->gravity.z};
        ImGui::InputFloat3("Gravity", gravity);

        ImGui::InputFloat("Frame Rate", &emitter->frameRate);

        int frames[2] = {(int)emitter->framesX, (int)emitter->framesY};
        ImGui::InputInt2("Frames", frames);

        ImGui::InputInt("Frame Count", (int*)&emitter->frameCount);
        ImGui::InputInt("Start Frame", (int*)&emitter->frameStart);
      }

      if (ImGui::CollapsingHeader("Debug")) {
        ImGui::SeparatorText("Shaders");
        if (ImGui::Button("Reload Shaders")) {
          my::LoadShaders();
          my::DoTest();
        }
      }

      ImGui::End();
    }

    // Rendering
    ImGui::Render();
    const float clear_color_with_alpha[4] = {
        clear_color.x * clear_color.w, clear_color.y * clear_color.w,
        clear_color.z * clear_color.w, clear_color.w};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView,
                                            nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView,
                                               clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0);  // Present with vsync
    // g_pSwapChain->Present(0, 0); // Present without vsync
  }

  // Cleanup
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  my::DeinitEngine();

  CleanupDeviceD3D();
  ::DestroyWindow(hwnd);
  ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

  return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd) {
  // Setup swap chain
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };
  HRESULT res = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
      featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
      &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
  if (res == DXGI_ERROR_UNSUPPORTED)  // Try high-performance WARP software
                                      // driver if hardware is not available.
    res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
  if (res != S_OK) return false;

  CreateRenderTarget();
  return true;
}

void CleanupDeviceD3D() {
  CleanupRenderTarget();
  if (g_pSwapChain) {
    g_pSwapChain->Release();
    g_pSwapChain = nullptr;
  }
  if (g_pd3dDeviceContext) {
    g_pd3dDeviceContext->Release();
    g_pd3dDeviceContext = nullptr;
  }
  if (g_pd3dDevice) {
    g_pd3dDevice->Release();
    g_pd3dDevice = nullptr;
  }
}

void CreateRenderTarget() {
  ID3D11Texture2D* pBackBuffer;
  g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr,
                                       &g_mainRenderTargetView);
  pBackBuffer->Release();
}

void CleanupRenderTarget() {
  if (g_mainRenderTargetView) {
    g_mainRenderTargetView->Release();
    g_mainRenderTargetView = nullptr;
  }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell
// if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to
// your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data
// to your main application, or clear/overwrite your copy of the keyboard
// data. Generally you may always pass all inputs to dear imgui, and hide them
// from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

  switch (msg) {
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED) return 0;
      g_ResizeWidth = (UINT)LOWORD(lParam);  // Queue resize
      g_ResizeHeight = (UINT)HIWORD(lParam);
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU)  // Disable ALT application menu
        return 0;
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      return 0;
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}