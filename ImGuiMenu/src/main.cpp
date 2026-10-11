// Win32 + DirectX11 host for menu.h
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <windowsx.h>
#include <chrono>
#include <thread>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "menu.h"

static ID3D11Device*           g_dev  = nullptr;
static ID3D11DeviceContext*    g_ctx  = nullptr;
static IDXGISwapChain*         g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv  = nullptr;
static bool                    g_occluded = false;
static UINT                    g_resW = 0, g_resH = 0;
static constexpr int           kPanelWidth = 880;
static constexpr int           kPanelHeight = 560;
static constexpr int           kOverlayToggleHotkeyId = 0xB417;
static int                     g_panelX = 0, g_panelY = 0;
static HWND                    g_glassHwnd = nullptr;
static bool                    g_glassAcrylic = false;

// SetWindowCompositionAttribute is dynamically resolved because it is not part of
// the stable public Win32 SDK surface on every toolchain.
enum AccentStateCustom {
    ACCENT_DISABLED_CUSTOM = 0,
    ACCENT_ENABLE_ACRYLICBLURBEHIND_CUSTOM = 4
};
enum WindowCompositionAttributeCustom {
    WCA_ACCENT_POLICY_CUSTOM = 19
};
struct AccentPolicyCustom {
    int AccentState;
    int AccentFlags;
    DWORD GradientColor; // AABBGGRR, including tint opacity in AA
    int AnimationId;
};
struct WindowCompositionAttributeDataCustom {
    int Attribute;
    PVOID Data;
    SIZE_T SizeOfData;
};
using SetWindowCompositionAttributeProc =
    BOOL(WINAPI*)(HWND, WindowCompositionAttributeDataCustom*);

static bool EnableAcrylicBackdrop(HWND hwnd)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setWindowCompositionAttribute = user32
        ? reinterpret_cast<SetWindowCompositionAttributeProc>(
            GetProcAddress(user32, "SetWindowCompositionAttribute"))
        : nullptr;
    if (!setWindowCompositionAttribute) return false;

    AccentPolicyCustom policy{};
    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND_CUSTOM;
    policy.AccentFlags = 2;
    policy.GradientColor = 0xAA191B29; // ARGB: dark, softly tinted glass
    WindowCompositionAttributeDataCustom data{};
    data.Attribute = WCA_ACCENT_POLICY_CUSTOM;
    data.Data = &policy;
    data.SizeOfData = sizeof(policy);
    return setWindowCompositionAttribute(hwnd, &data) != FALSE;
}

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static bool CreateRenderTarget();
static void CleanupRenderTarget();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
static LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, hInst, nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"ImGuiMenuWnd", nullptr };
    if (!RegisterClassExW(&wc)) return 1;

    // Full-screen host window; only the centered ImGui panel is painted.
    const int cw = GetSystemMetrics(SM_CXSCREEN);
    const int ch = GetSystemMetrics(SM_CYSCREEN);
    g_panelX = cw > kPanelWidth ? (cw - kPanelWidth) / 2 : 0;
    g_panelY = ch > kPanelHeight ? (ch - kPanelHeight) / 2 : 0;
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        wc.lpszClassName, L"ImGui Overlay", WS_POPUP,
        0, 0, cw, ch, nullptr, nullptr, hInst, nullptr);

    if (!hwnd) {
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }
    // Every pure-black pixel is transparent, including the area outside the panel.
    if (!SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY)) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }
    // Separate backdrop window: DWM acrylic blurs the desktop behind the panel,
    // while the DirectX color-key window keeps the rest of the screen transparent.
    g_glassHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        wc.lpszClassName, L"ImGui Glass Backdrop", WS_POPUP,
        g_panelX, g_panelY, kPanelWidth, kPanelHeight,
        nullptr, nullptr, hInst, nullptr);
    if (g_glassHwnd) {
        HRGN roundedRegion = CreateRoundRectRgn(0, 0, kPanelWidth + 1, kPanelHeight + 1, 28, 28);
        if (roundedRegion && !SetWindowRgn(g_glassHwnd, roundedRegion, TRUE))
            DeleteObject(roundedRegion);

        g_glassAcrylic = EnableAcrylicBackdrop(g_glassHwnd);
        if (!g_glassAcrylic) {
            // Compatibility fallback for Windows builds where acrylic is unavailable.
            LONG_PTR exStyle = GetWindowLongPtrW(g_glassHwnd, GWL_EXSTYLE);
            SetWindowLongPtrW(g_glassHwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            SetLayeredWindowAttributes(g_glassHwnd, 0, 224, LWA_ALPHA);
            OutputDebugStringW(L"Acrylic backdrop unavailable; using translucent fallback.\n");
        }
        ShowWindow(g_glassHwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(g_glassHwnd, hwnd, g_panelX, g_panelY, kPanelWidth, kPanelHeight,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        OutputDebugStringW(L"Glass backdrop window could not be created.\n");
    }

    // Register a global Insert hotkey so the overlay can be reopened while hidden.
    const bool insertHotkeyRegistered =
        RegisterHotKey(hwnd, kOverlayToggleHotkeyId, MOD_NOREPEAT, VK_INSERT) != FALSE;
    if (!insertHotkeyRegistered)
        OutputDebugStringW(L"RegisterHotKey(INSERT) failed; using key-state fallback.\n");

    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, cw, ch,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    const bool win32Ready = ImGui_ImplWin32_Init(hwnd);
    const bool dx11Ready = win32Ready && ImGui_ImplDX11_Init(g_dev, g_ctx);
    if (!dx11Ready) {
        if (win32Ready) ImGui_ImplWin32_Shutdown();
        if (insertHotkeyRegistered) UnregisterHotKey(hwnd, kOverlayToggleHotkeyId);
        if (g_glassHwnd) { DestroyWindow(g_glassHwnd); g_glassHwnd = nullptr; }
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, hInst);
        return 1;
    }

    // Segoe UI ships with Windows; falls back to the built-in font if missing.
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.0f))
        io.Fonts->AddFontDefault();

    menu::Style();
    menu::State state;
    state.panel_x = static_cast<float>(g_panelX);
    state.panel_y = static_cast<float>(g_panelY);
    bool show = true;

    auto toggleOverlay = [&]() {
        show = !show;
        if (show) {
            if (g_glassHwnd) {
                ShowWindow(g_glassHwnd, SW_SHOWNOACTIVATE);
                SetWindowPos(g_glassHwnd, hwnd, g_panelX, g_panelY,
                             kPanelWidth, kPanelHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
            ShowWindow(hwnd, SW_SHOW);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, cw, ch,
                         SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
        } else {
            ShowWindow(hwnd, SW_HIDE);
            if (g_glassHwnd) ShowWindow(g_glassHwnd, SW_HIDE);
        }
    };
    bool insertWasDown = false;
    const float clear[4] = { 0.f, 0.f, 0.f, 0.f }; // black color-key background = transparent

    using Clock = std::chrono::steady_clock;
    auto fpsWindowStart = Clock::now();
    auto nextFrame = fpsWindowStart;
    unsigned long long fpsFrameCount = 0;
    const auto frameInterval = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / 144.0)); // smooth UI without an unbounded busy loop
    const MMRESULT timerResolution = timeBeginPeriod(1);

    bool done = false;
    while (!done) {
        if (!insertHotkeyRegistered) {
            const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
            if (insertDown && !insertWasDown) toggleOverlay();
            insertWasDown = insertDown;
        }

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (insertHotkeyRegistered && msg.message == WM_HOTKEY &&
                msg.wParam == static_cast<WPARAM>(kOverlayToggleHotkeyId)) {
                toggleOverlay();
            }
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        // Stay alive while hidden so the global Insert hotkey can reopen the overlay.
        if (!show) {
            if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE);
            if (g_glassHwnd && IsWindowVisible(g_glassHwnd)) ShowWindow(g_glassHwnd, SW_HIDE);
            Sleep(10);
            continue;
        }

        if (IsIconic(hwnd)) { Sleep(50); continue; }
        if (g_occluded && g_swap->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) { Sleep(50); continue; }
        g_occluded = false;

        if (g_resW != 0 && g_resH != 0) {
            CleanupRenderTarget();
            const HRESULT resizeHr = g_swap->ResizeBuffers(0, g_resW, g_resH, DXGI_FORMAT_UNKNOWN, 0);
            g_resW = g_resH = 0;
            if (FAILED(resizeHr) || !CreateRenderTarget()) {
                OutputDebugStringW(L"DirectX resize/render target failure.\n");
                done = true;
                break;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (show) {
            menu::Render(state, &show);
            const int movedX = static_cast<int>(state.panel_x);
            const int movedY = static_cast<int>(state.panel_y);
            if (movedX != g_panelX || movedY != g_panelY) {
                g_panelX = movedX;
                g_panelY = movedY;
                if (g_glassHwnd && IsWindowVisible(g_glassHwnd))
                    SetWindowPos(g_glassHwnd, hwnd, g_panelX, g_panelY,
                                 kPanelWidth, kPanelHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
        }
        if (state.request_minimize) {
            state.request_minimize = false;
            show = false;
            ShowWindow(hwnd, SW_HIDE);
        }
        // The close button hides the menu; Insert always brings it back.
        if (!show) ShowWindow(hwnd, SW_HIDE);

        ImGui::Render();
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // No VSync: don't pin the counter to the monitor's 75 Hz refresh rate.
        HRESULT hr = g_swap->Present(0, 0);
        g_occluded = (hr == DXGI_STATUS_OCCLUDED);
        if (FAILED(hr) && !g_occluded) {
            OutputDebugStringW(L"DirectX Present failed.\n");
            done = true;
        }

        // Reuse one timestamp for FPS sampling and frame pacing.
        const auto now = Clock::now();
        // Count only successful, visible Presents rather than failed/occluded attempts.
        if (SUCCEEDED(hr) && !g_occluded) {
            ++fpsFrameCount;
            const double elapsed = std::chrono::duration<double>(now - fpsWindowStart).count();
            if (elapsed >= 0.5) {
                state.render_fps = static_cast<float>(fpsFrameCount / elapsed);
                fpsFrameCount = 0;
                fpsWindowStart = now;
            }
        }

        // Limit to 144 updates/s to keep animations smooth while avoiding a spin loop.
        nextFrame += frameInterval;
        if (nextFrame > now)
            std::this_thread::sleep_until(nextFrame);
        else
            nextFrame = now;
    }

    if (timerResolution == TIMERR_NOERROR) timeEndPeriod(1);
    if (insertHotkeyRegistered) UnregisterHotKey(hwnd, kOverlayToggleHotkeyId);
    if (g_glassHwnd) { DestroyWindow(g_glassHwnd); g_glassHwnd = nullptr; }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInst);
    return 0;
}

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL fls[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fls, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &fl, &g_ctx);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fls, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &fl, &g_ctx);
    if (FAILED(hr)) return false;
    return CreateRenderTarget();
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx)  { g_ctx->Release();  g_ctx  = nullptr; }
    if (g_dev)  { g_dev->Release();  g_dev  = nullptr; }
}

static bool CreateRenderTarget()
{
    if (!g_swap || !g_dev) return false;
    ID3D11Texture2D* bb = nullptr;
    const HRESULT getHr = g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (FAILED(getHr) || !bb) return false;
    const HRESULT viewHr = g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
    return SUCCEEDED(viewHr) && g_rtv != nullptr;
}

static void CleanupRenderTarget()
{
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd == g_glassHwnd) {
        switch (msg) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND:
            if (!g_glassAcrylic) {
                RECT rc{};
                GetClientRect(hWnd, &rc);
                HBRUSH brush = CreateSolidBrush(RGB(25, 27, 41));
                if (brush) {
                    FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
                    DeleteObject(brush);
                }
            }
            return 1;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    if (ImGui::GetCurrentContext() != nullptr &&
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_resW = (UINT)LOWORD(lParam);
        g_resH = (UINT)HIWORD(lParam);
        return 0;
    case WM_NCHITTEST: {
        // Let mouse clicks pass through everywhere outside the visible panel.
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &p);
        const int panelLeft = g_panelX;
        const int panelTop = g_panelY;
        if (p.x < panelLeft || p.x >= panelLeft + kPanelWidth ||
            p.y < panelTop || p.y >= panelTop + kPanelHeight)
            return HTTRANSPARENT;
        return HTCLIENT;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
