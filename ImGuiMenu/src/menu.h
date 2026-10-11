// menu.h - single-header ImGui recreation of the "Aim" settings window.
// Backend-agnostic (DX11/DX9/GL/Vulkan). Needs Dear ImGui 1.88+.
// Usage:
//   static menu::State st; static bool show = true;
//   menu::Style();                 // once, after CreateContext + font load
//   if (show) menu::Render(st, &show);   // every frame between NewFrame/Render
//
// Font: load Inter / SF Pro / Segoe UI Variable at ~15px for the look in the screenshot.

#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cctype>

namespace menu {

// ------------------------------------------------------------ state
struct State {
    int  tab = 0;
    bool compact_mode = false;
    bool show_fps = true;
    float render_fps = 0.0f; // measured by the host render loop
    bool keyboard_navigation = true;
    float ui_scale = 1.0f;
    int player_filter = 0;
    char profile_name[48] = "Default";
    bool show_help = false;
    bool request_minimize = false;
    bool animations_enabled = true;
    bool include_offline_players = false;
    char search[64] = {};

    // Movable panel position and drag origin (screen coordinates).
    float panel_x = -1.f;
    float panel_y = -1.f;
    bool panel_dragging = false;
    ImVec2 panel_drag_start_mouse = ImVec2(0.f, 0.f);
    float panel_drag_start_x = 0.f;
    float panel_drag_start_y = 0.f;

    // aimbot
    bool  aim_enabled = true, visible_only = true, team_check = false, draw_fov = true;
    float smoothing = 6.f, fov = 120.f;
    int   hitbox = 0;
    ImGuiKey aim_key = ImGuiKey_MouseRight;
    int   preset = 0;

    // esp
    bool  esp_enabled = true, box = true, name = true, health = true, distance = false, skeleton = false;
    float box_col[3]  = { 0.04f, 0.52f, 1.00f };
    float name_col[3] = { 0.95f, 0.95f, 0.97f };
    float skel_col[3] = { 1.00f, 0.23f, 0.19f };
    float max_dist = 1400.f;
    int   box_style = 0;
    int   font_size = 12;
};

// ------------------------------------------------------------ constants
static const ImU32 ACCENT = IM_COL32(10, 132, 255, 255);
static const ImU32 GREEN  = IM_COL32(48, 209, 88, 255);
static const ImU32 COL_TEXT   = IM_COL32(245, 245, 247, 255);
static const ImU32 DIM    = IM_COL32(235, 235, 245, 150);
static const ImU32 FILL   = IM_COL32(255, 255, 255, 22);
static const ImU32 LINE   = IM_COL32(255, 255, 255, 38);
static constexpr float ROW_H = 37.f;
static constexpr float PAD   = 14.f;
static bool g_animationsEnabled = true;
static float g_rowHeight = ROW_H;

// ------------------------------------------------------------ helpers
inline ImU32 LerpCol(ImU32 a, ImU32 b, float t) {
    ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t,
                                                 x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

inline float Anim(ImGuiID id, bool target, float speed = 14.f) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    const float desired = target ? 1.f : 0.f;
    float v = st->GetFloat(id, desired);
    if (!g_animationsEnabled) {
        st->SetFloat(id, desired);
        return desired;
    }
    const float blend = 1.f - expf(-speed * ImMax(0.f, ImGui::GetIO().DeltaTime));
    v += (desired - v) * blend;
    st->SetFloat(id, v);
    return v;
}

inline float SmoothValue(ImGuiID id, float target, float speed = 22.f) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float value = st->GetFloat(id, target);
    if (!g_animationsEnabled) {
        st->SetFloat(id, target);
        return target;
    }
    const float blend = 1.f - expf(-speed * ImMax(0.f, ImGui::GetIO().DeltaTime));
    value += (target - value) * blend;
    st->SetFloat(id, value);
    return value;
}

inline void Chevron(ImDrawList* dl, ImVec2 c, bool up, ImU32 col) {
    float d = up ? 1.f : -1.f;
    ImVec2 p[3] = { { c.x - 3.f, c.y + 1.5f * d }, { c.x, c.y - 1.5f * d }, { c.x + 3.f, c.y + 1.5f * d } };
    dl->AddPolyline(p, 3, col, ImDrawFlags_None, 1.5f);
}

// ------------------------------------------------------------ card + rows
struct Card { ImDrawListSplitter sp; ImVec2 p0; float w; };
struct RowCtx { ImRect r; float x; ImDrawList* dl; float cy; };

inline void BeginCard(Card& c, const char* title) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddText(ImGui::GetFont(), 13.f, ImVec2(p.x + 4.f, p.y + 1.f), DIM, title);
    ImGui::Dummy(ImVec2(1.f, 23.f));
    c.p0 = ImGui::GetCursorScreenPos();
    c.w  = ImGui::GetContentRegionAvail().x;
    c.sp.Split(dl, 2);
    c.sp.SetCurrentChannel(dl, 1);
}

inline void EndCard(Card& c, const char* footnote = nullptr) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p1(c.p0.x + c.w, ImGui::GetCursorScreenPos().y);
    // Keep the acrylic desktop material visible through the cards.
    // Borders define each section without painting an opaque rectangle over the glass.
    c.sp.SetCurrentChannel(dl, 0);
    dl->AddRect(c.p0, p1, IM_COL32(235, 240, 255, 46), 12.f, 0, 1.f);
    dl->AddLine(ImVec2(c.p0.x + 14.f, c.p0.y + 1.f),
                ImVec2(c.p0.x + c.w - 14.f, c.p0.y + 1.f),
                IM_COL32(255, 255, 255, 24), 1.f);
    c.sp.Merge(dl);
    if (footnote) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddText(ImGui::GetFont(), 14.f, ImVec2(p.x + 14.f, p.y + 10.f), DIM, footnote);
        ImGui::Dummy(ImVec2(1.f, 30.f));
    }
}

inline RowCtx Row(const char* label, bool first = false) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    RowCtx c;
    c.r  = ImRect(p, ImVec2(p.x + w, p.y + g_rowHeight));
    c.x  = c.r.Max.x - PAD;
    c.dl = dl;
    c.cy = p.y + g_rowHeight * 0.5f;
    if (!first) dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y), IM_COL32(255, 255, 255, 18));
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p.x + PAD, c.cy - ts.y * 0.5f), COL_TEXT, label);
    ImGui::Dummy(ImVec2(w, g_rowHeight));
    return c;
}

// ------------------------------------------------------------ controls (laid out right -> left)
inline bool Switch(RowCtx& c, bool* v) {
    const float W = 44.f, H = 26.f;
    ImVec2 p0(c.x - W, c.cy - H * 0.5f), save = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(v);
    bool hit = ImGui::InvisibleButton("sw", ImVec2(W, H));
    if (hit) *v = !*v;
    float t = Anim(ImGui::GetItemID(), *v);
    ImGui::PopID();
    c.dl->AddRectFilled(p0, ImVec2(p0.x + W, p0.y + H), LerpCol(IM_COL32(72, 72, 76, 255), GREEN, t), H * 0.5f);
    ImVec2 k(p0.x + H * 0.5f + (W - H) * t, c.cy);
    c.dl->AddCircleFilled(ImVec2(k.x, k.y + 1.f), H * 0.5f - 1.f, IM_COL32(0, 0, 0, 50), 24);
    c.dl->AddCircleFilled(k, H * 0.5f - 2.f, IM_COL32_WHITE, 24);
    ImGui::SetCursorScreenPos(save);
    c.x -= W + 10.f;
    return hit;
}

inline void ColorDot(RowCtx& c, float* col) {
    const float D = 24.f;
    ImVec2 ctr(c.x - D * 0.5f, c.cy), save = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(ctr.x - D * 0.5f, ctr.y - D * 0.5f));
    ImGui::PushID(col);
    if (ImGui::InvisibleButton("dot", ImVec2(D, D))) ImGui::OpenPopup("cp");
    ImU32 u = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], 1.f));
    c.dl->AddCircleFilled(ctr, 11.f, u, 24);
    c.dl->AddCircle(ctr, 11.f, IM_COL32(255, 255, 255, 50), 24, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    bool o = ImGui::BeginPopup("cp");
    ImGui::PopStyleVar();
    if (o) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
        ImGui::ColorPicker3("##p", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(save);
    c.x -= D + 10.f;
}

inline bool Slider(RowCtx& c, float* v, float mn, float mx, const char* fmt, bool integer = false) {
    if (!v || !std::isfinite(mn) || !std::isfinite(mx) || mn >= mx) return false;
    if (!std::isfinite(*v)) *v = mn;
    *v = ImClamp(*v, mn, mx);
    const float TW = 40.f, TRACK = 96.f, KR = 9.f;
    char buf[16]; snprintf(buf, sizeof buf, fmt, *v);
    ImVec2 ts = ImGui::CalcTextSize(buf), save = ImGui::GetCursorScreenPos();
    c.dl->AddText(ImVec2(c.x - ts.x, c.cy - ts.y * 0.5f), DIM, buf);
    float tx1 = c.x - TW, tx0 = tx1 - TRACK;
    ImGui::SetCursorScreenPos(ImVec2(tx0 - KR, c.cy - KR - 2.f));
    ImGui::PushID(v);
    ImGui::InvisibleButton("sl", ImVec2(TRACK + KR * 2.f, KR * 2.f + 4.f));
    const ImGuiID sliderId = ImGui::GetItemID();
    bool changed = false;
    if (ImGui::IsItemActive()) {
        float t = ImClamp((ImGui::GetIO().MousePos.x - tx0) / TRACK, 0.f, 1.f);
        float nv = mn + (mx - mn) * t;
        if (integer) nv = roundf(nv);
        if (nv != *v) { *v = nv; changed = true; }
    }
    ImGui::PopID();
    const float targetT = ImClamp((*v - mn) / (mx - mn), 0.f, 1.f);
    const float t = SmoothValue(sliderId ^ 0x3D17A2B9u, targetT, 24.f);
    const float kx = tx0 + TRACK * t;
    c.dl->AddRectFilled(ImVec2(tx0, c.cy - 2.f), ImVec2(tx1, c.cy + 2.f), IM_COL32(255, 255, 255, 40), 2.f);
    c.dl->AddRectFilled(ImVec2(tx0, c.cy - 2.f), ImVec2(kx, c.cy + 2.f), ACCENT, 2.f);
    c.dl->AddCircleFilled(ImVec2(kx, c.cy + 1.f), KR, IM_COL32(0, 0, 0, 60), 24);
    c.dl->AddCircleFilled(ImVec2(kx, c.cy), KR, IM_COL32_WHITE, 24);
    ImGui::SetCursorScreenPos(save);
    c.x -= TW + TRACK + 12.f;
    return changed;
}

inline bool Segmented(RowCtx& c, const char* const* items, int n, int* sel) {
    const float H = 30.f, PX = 12.f;
    if (!items || !sel || n <= 0 || n > 8) return false;
    *sel = ImClamp(*sel, 0, n - 1);
    float w[8], total = 0.f;
    for (int i = 0; i < n; i++) { w[i] = ImGui::CalcTextSize(items[i]).x + PX * 2.f; total += w[i]; }
    ImVec2 p0(c.x - total - 4.f, c.cy - H * 0.5f), save = ImGui::GetCursorScreenPos();
    c.dl->AddRectFilled(p0, ImVec2(p0.x + total + 4.f, p0.y + H), IM_COL32(0, 0, 0, 70), 8.f);
    bool changed = false;
    float cur = p0.x + 2.f;
    ImGui::PushID(sel);
    for (int i = 0; i < n; i++) {
        ImVec2 a(cur, p0.y + 2.f), b(cur + w[i], p0.y + H - 2.f);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("seg", ImVec2(b.x - a.x, b.y - a.y))) { *sel = i; changed = true; }
        float t = Anim(ImGui::GetItemID(), *sel == i);
        ImGui::PopID();
        c.dl->AddRectFilled(a, b, LerpCol(IM_COL32(120, 120, 128, 0), IM_COL32(120, 120, 128, 210), t), 6.f);
        ImVec2 ts = ImGui::CalcTextSize(items[i]);
        c.dl->AddText(ImVec2(a.x + (b.x - a.x - ts.x) * 0.5f, c.cy - ts.y * 0.5f), COL_TEXT, items[i]);
        cur += w[i];
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(save);
    c.x -= total + 4.f + 10.f;
    return changed;
}

inline bool Combo(RowCtx& c, const char* const* items, int n, int* sel, float W = 120.f) {
    if (!items || !sel || n <= 0 || W <= 0.f) return false;
    *sel = ImClamp(*sel, 0, n - 1);
    const float H = 28.f;
    ImVec2 p0(c.x - W, c.cy - H * 0.5f), p1(c.x, c.cy + H * 0.5f), save = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(sel);
    if (ImGui::InvisibleButton("cb", ImVec2(W, H))) ImGui::OpenPopup("dd");
    c.dl->AddRectFilled(p0, p1, FILL, 8.f);
    c.dl->AddRect(p0, p1, LINE, 8.f);
    ImVec2 ts = ImGui::CalcTextSize(items[*sel]);
    c.dl->AddText(ImVec2(p0.x + 12.f, c.cy - ts.y * 0.5f), COL_TEXT, items[*sel]);
    ImVec2 b0(p1.x - 24.f, p0.y + 3.f), b1(p1.x - 3.f, p1.y - 3.f);
    c.dl->AddRectFilled(b0, b1, ACCENT, 6.f);
    ImVec2 m((b0.x + b1.x) * 0.5f, c.cy);
    Chevron(c.dl, ImVec2(m.x, m.y - 4.f), true, IM_COL32_WHITE);
    Chevron(c.dl, ImVec2(m.x, m.y + 4.f), false, IM_COL32_WHITE);

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(p0.x, p1.y + 4.f));
    ImGui::SetNextWindowSize(ImVec2(W, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    bool o = ImGui::BeginPopup("dd");
    ImGui::PopStyleVar();
    if (o) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
        for (int i = 0; i < n; i++)
            if (ImGui::Selectable(items[i], i == *sel)) { *sel = i; changed = true; }
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(save);
    c.x -= W + 10.f;
    return changed;
}

inline bool Stepper(RowCtx& c, int* v, int mn, int mx) {
    if (!v || mn > mx) return false;
    *v = ImClamp(*v, mn, mx);
    const float W = 26.f, H = 32.f;
    ImVec2 p0(c.x - W, c.cy - H * 0.5f), p1(c.x, c.cy + H * 0.5f), save = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(v);
    bool hit = ImGui::InvisibleButton("st", ImVec2(W, H)), changed = false;
    if (hit) {
        int nv = ImGui::GetIO().MousePos.y < c.cy ? *v + 1 : *v - 1;
        nv = ImClamp(nv, mn, mx);
        if (nv != *v) { *v = nv; changed = true; }
    }
    ImGui::PopID();
    c.dl->AddRectFilled(p0, p1, FILL, 8.f);
    c.dl->AddRect(p0, p1, LINE, 8.f);
    Chevron(c.dl, ImVec2(c.x - W * 0.5f, c.cy - 7.f), true, COL_TEXT);
    Chevron(c.dl, ImVec2(c.x - W * 0.5f, c.cy + 7.f), false, COL_TEXT);
    char buf[8]; snprintf(buf, sizeof buf, "%d", *v);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    c.dl->AddText(ImVec2(p0.x - 14.f - ts.x, c.cy - ts.y * 0.5f), DIM, buf);
    ImGui::SetCursorScreenPos(save);
    c.x -= W + 40.f;
    return changed;
}

inline bool KeyBind(RowCtx& c, ImGuiKey* key) {
    static ImGuiKey* waiting = nullptr;
    static int armed = -1;
    const char* nm = (waiting == key) ? "..." : ImGui::GetKeyName(*key);
    const float H = 28.f;
    float W = ImMax(90.f, ImGui::CalcTextSize(nm).x + 28.f);
    ImVec2 p0(c.x - W, c.cy - H * 0.5f), p1(c.x, c.cy + H * 0.5f), save = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(key);
    if (ImGui::InvisibleButton("kb", ImVec2(W, H))) { waiting = key; armed = ImGui::GetFrameCount(); }
    ImGui::PopID();
    c.dl->AddRectFilled(p0, p1, FILL, 8.f);
    c.dl->AddRect(p0, p1, waiting == key ? ACCENT : LINE, 8.f);
    ImVec2 ts = ImGui::CalcTextSize(nm);
    c.dl->AddText(ImVec2(p0.x + (W - ts.x) * 0.5f, c.cy - ts.y * 0.5f), COL_TEXT, nm);

    bool changed = false;
    if (waiting == key && ImGui::GetFrameCount() > armed) {
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
            if (k >= ImGuiKey_GamepadStart && k <= ImGuiKey_GamepadRStickRight) continue;
            if (k == ImGuiKey_MouseWheelX || k == ImGuiKey_MouseWheelY) continue;
            if (ImGui::IsKeyPressed((ImGuiKey)k, false)) {
                if (k != ImGuiKey_Escape) { *key = (ImGuiKey)k; changed = true; }
                waiting = nullptr;
                break;
            }
        }
    }
    ImGui::SetCursorScreenPos(save);
    c.x -= W + 10.f;
    return changed;
}

// ------------------------------------------------------------ sidebar bits
inline void Icon(ImDrawList* dl, ImVec2 c, int id, ImU32 bg) {
    const float h = 11.f; const ImU32 w = IM_COL32_WHITE;
    dl->AddRectFilled(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), bg, 6.f);
    switch (id) {
    case 0: // aim
        dl->AddCircle(c, 5.5f, w, 0, 1.6f);
        dl->AddCircleFilled(c, 1.5f, w);
        dl->AddLine(ImVec2(c.x, c.y - 8.f), ImVec2(c.x, c.y - 4.f), w, 1.6f);
        dl->AddLine(ImVec2(c.x, c.y + 4.f), ImVec2(c.x, c.y + 8.f), w, 1.6f);
        dl->AddLine(ImVec2(c.x - 8.f, c.y), ImVec2(c.x - 4.f, c.y), w, 1.6f);
        dl->AddLine(ImVec2(c.x + 4.f, c.y), ImVec2(c.x + 8.f, c.y), w, 1.6f);
        break;
    case 1: // eye
        dl->AddBezierQuadratic(ImVec2(c.x - 8.f, c.y), ImVec2(c.x, c.y - 9.f), ImVec2(c.x + 8.f, c.y), w, 1.6f, 16);
        dl->AddBezierQuadratic(ImVec2(c.x - 8.f, c.y), ImVec2(c.x, c.y + 9.f), ImVec2(c.x + 8.f, c.y), w, 1.6f, 16);
        dl->AddCircle(c, 2.4f, w, 0, 1.6f);
        break;
    case 2: // players
        dl->AddCircle(ImVec2(c.x, c.y - 3.5f), 3.f, w, 0, 1.6f);
        dl->AddBezierQuadratic(ImVec2(c.x - 6.5f, c.y + 6.f), ImVec2(c.x, c.y - 3.f), ImVec2(c.x + 6.5f, c.y + 6.f), w, 1.6f, 16);
        break;
    case 3: // misc
        dl->AddLine(ImVec2(c.x - 6.f, c.y - 3.5f), ImVec2(c.x + 6.f, c.y - 3.5f), w, 1.6f);
        dl->AddLine(ImVec2(c.x - 6.f, c.y + 3.5f), ImVec2(c.x + 6.f, c.y + 3.5f), w, 1.6f);
        dl->AddCircleFilled(ImVec2(c.x + 2.f, c.y - 3.5f), 2.2f, w);
        dl->AddCircleFilled(ImVec2(c.x - 2.f, c.y + 3.5f), 2.2f, w);
        break;
    default: // config / folder
        dl->AddRect(ImVec2(c.x - 6.f, c.y - 4.f), ImVec2(c.x + 6.f, c.y + 5.f), w, 2.f, 0, 1.6f);
        dl->AddLine(ImVec2(c.x - 6.f, c.y - 4.f), ImVec2(c.x - 2.f, c.y - 6.f), w, 1.6f);
        break;
    }
}

inline bool Tab(const char* label, int icon, ImU32 col, bool sel, float w) {
    const float H = 30.f;
    ImVec2 p0 = ImGui::GetCursorScreenPos(), p1(p0.x + w, p0.y + H);
    bool hit = ImGui::InvisibleButton(label, ImVec2(w, H));
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiID itemId = ImGui::GetItemID();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float selectedT = Anim(itemId ^ 0x14AA31u, sel, 13.f);
    const float hoverT = Anim(itemId ^ 0x5C39D7u, hovered && !sel, 18.f);
    if (hoverT > 0.005f)
        dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, static_cast<int>(24.f * hoverT)), 8.f);
    if (selectedT > 0.005f) {
        dl->AddRectFilled(p0, p1,
            IM_COL32(10, 132, 255, static_cast<int>(232.f * selectedT)), 8.f);
        dl->AddRectFilled(ImVec2(p0.x + 4.f, p0.y + 6.f),
                          ImVec2(p0.x + 7.f, p1.y - 6.f),
                          IM_COL32(255, 255, 255, static_cast<int>(210.f * selectedT)), 2.f);
    }

    Icon(dl, ImVec2(p0.x + 19.f, p0.y + H * 0.5f), icon, LerpCol(col, COL_TEXT, selectedT * 0.72f));
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(p0.x + 40.f, p0.y + (H - ts.y) * 0.5f), COL_TEXT, label);
    return hit;
}

inline void DrawBackground(ImDrawList* dl, ImVec2 p, ImVec2 s) {
    // The separate DWM acrylic window paints the blurred glass behind this UI.
    // Leave the panel interior untouched so the desktop material can show through.
    ImVec2 q(p.x + s.x, p.y + s.y);
    dl->AddRect(p, q, IM_COL32(238, 242, 255, 78), 14.f, 0, 1.15f);
    dl->AddRect(ImVec2(p.x + 1.f, p.y + 1.f),
                ImVec2(q.x - 1.f, q.y - 1.f),
                IM_COL32(255, 255, 255, 22), 13.f, 0, 1.f);

    // Clean glass separation for the navigation rail, plus a restrained top highlight.
    dl->AddLine(ImVec2(p.x + 200.f, p.y + 14.f),
                ImVec2(p.x + 200.f, q.y - 14.f),
                IM_COL32(238, 242, 255, 48), 1.f);
    dl->AddLine(ImVec2(p.x + 28.f, p.y + 1.5f),
                ImVec2(q.x - 28.f, p.y + 1.5f),
                IM_COL32(150, 190, 255, 90), 1.f);
}

// ------------------------------------------------------------ pages
inline void AimPage(State& s, float verticalOffset = 0.f) {
    static const char* hitboxes[] = { "Head", "Neck", "Chest" };
    static const char* presets[]  = { "Legit", "Semi", "Custom" };

    const float colw = 306.f;
    ImGui::SetCursorPos(ImVec2(25.f, 87.f + verticalOffset));

    // ---- aim controls
    ImGui::BeginChild("##col1", ImVec2(colw, 420.f), 0,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    {
        Card cd; BeginCard(cd, "AIM ASSIST");
        { auto r = Row("Enabled", true); Switch(r, &s.aim_enabled); }
        { auto r = Row("Smoothing");     Slider(r, &s.smoothing, 1.f, 10.f, "%.1f"); }
        { auto r = Row("FOV");           Slider(r, &s.fov, 1.f, 360.f, "%.0f", true); }
        { auto r = Row("Hitbox");        Segmented(r, hitboxes, 3, &s.hitbox); }
        { auto r = Row("Aim key");       KeyBind(r, &s.aim_key); }
        { auto r = Row("Preset");        Combo(r, presets, 3, &s.preset); }
        EndCard(cd, "Controls shown here are UI settings only.");
    }
    ImGui::EndChild();

    ImGui::SameLine(0.f, 17.f);

    // ---- targeting options and a live visual preview
    ImGui::BeginChild("##col2", ImVec2(colw, 420.f), 0,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    {
        Card cd; BeginCard(cd, "TARGET FILTERS");
        { auto r = Row("Visible only", true); Switch(r, &s.visible_only); }
        { auto r = Row("Team check");         Switch(r, &s.team_check); }
        { auto r = Row("Draw FOV circle");    Switch(r, &s.draw_fov); }
        EndCard(cd, "No game data source is connected.");

        ImGui::Dummy(ImVec2(1.f, 12.f));
        const ImVec2 previewPos = ImGui::GetCursorScreenPos();
        const ImVec2 previewSize(306.f, 136.f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(previewPos, ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y), FILL, 12.f);
        dl->AddRect(previewPos, ImVec2(previewPos.x + previewSize.x, previewPos.y + previewSize.y), LINE, 12.f);
        ImGui::Dummy(previewSize);

        const ImVec2 center(previewPos.x + 70.f, previewPos.y + 72.f);
        const float radius = 18.f + 42.f * ImClamp(s.fov / 360.f, 0.f, 1.f);
        if (s.draw_fov) {
            dl->AddCircle(center, radius, ACCENT, 64, 1.7f);
            dl->AddCircle(center, radius + 3.f, IM_COL32(10, 132, 255, 45), 64, 1.f);
        }
        dl->AddCircleFilled(center, 2.5f, COL_TEXT, 16);
        dl->AddLine(ImVec2(center.x - 8.f, center.y), ImVec2(center.x + 8.f, center.y), COL_TEXT, 1.f);
        dl->AddLine(ImVec2(center.x, center.y - 8.f), ImVec2(center.x, center.y + 8.f), COL_TEXT, 1.f);
        dl->AddText(ImVec2(previewPos.x + 130.f, previewPos.y + 30.f), COL_TEXT, "FOV preview");
        char fovText[32];
        snprintf(fovText, sizeof fovText, "%.0f degrees", s.fov);
        dl->AddText(ImVec2(previewPos.x + 130.f, previewPos.y + 54.f), DIM, fovText);
        dl->AddText(ImVec2(previewPos.x + 130.f, previewPos.y + 80.f), DIM, "Visual only");
    }
    ImGui::EndChild();
}

inline bool MatchesSearch(const char* label, const char* query) {
    if (!query || !*query) return true;
    if (!label) return false;
    for (const char* p = label; *p; ++p) {
        const char* a = p;
        const char* b = query;
        while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b))) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}

// ------------------------------------------------------------ independent demonstration pages
inline void SecondaryPage(State& s, float verticalOffset = 0.f) {
    ImGui::SetCursorPos(ImVec2(25.f, 98.f + verticalOffset));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.f, 12.f));

    static State savedProfile{};
    static bool hasSavedProfile = false;
    static const char* boxStyles[] = { "Corners", "Full", "Filled" };
    static const char* playerFilters[] = { "All", "Online", "Offline" };

    if (s.tab == 1) {
        ImGui::BeginChild("##visualsLeft", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "ESP OVERLAY");
            { auto r = Row("Enabled", true);    Switch(r, &s.esp_enabled); }
            { auto r = Row("Bounding boxes");   Switch(r, &s.box); }
            { auto r = Row("Player names");     Switch(r, &s.name); }
            { auto r = Row("Health bars");      Switch(r, &s.health); }
            { auto r = Row("Distance labels");  Switch(r, &s.distance); }
            { auto r = Row("Skeleton preview"); Switch(r, &s.skeleton); }
            { auto r = Row("Max distance");     Slider(r, &s.max_dist, 0.f, 2000.f, "%.0f", true); }
            { auto r = Row("Box style");        Combo(r, boxStyles, 3, &s.box_style, 116.f); }
            EndCard(cd, "Visual controls are stored locally in this UI.");
        }
        ImGui::EndChild();
        ImGui::SameLine(0.f, 17.f);
        ImGui::BeginChild("##visualsRight", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "COLORS & DISPLAY");
            { auto r = Row("Box color", true);      ColorDot(r, s.box_col); }
            { auto r = Row("Name color");           ColorDot(r, s.name_col); }
            { auto r = Row("Skeleton color");       ColorDot(r, s.skel_col); }
            { auto r = Row("Overlay font size");    Stepper(r, &s.font_size, 8, 32); }
            { auto r = Row("Compact UI density");   Switch(r, &s.compact_mode); }
            EndCard(cd, "Click each color dot to open the picker.");
        }
        ImGui::EndChild();
    } else if (s.tab == 2) {
        ImGui::BeginChild("##playersLeft", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "PLAYER FILTERS");
            { auto r = Row("List filter", true); Segmented(r, playerFilters, 3, &s.player_filter); }
            { auto r = Row("Include offline");    Switch(r, &s.include_offline_players); }
            ImGui::Dummy(ImVec2(1.f, 14.f));
            ImGui::TextDisabled("Filters are ready for a player-data source.");
            EndCard(cd, "No source is connected in this UI demo.");
        }
        ImGui::EndChild();
        ImGui::SameLine(0.f, 17.f);
        ImGui::BeginChild("##playersRight", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "PLAYER LIST");
            ImGui::Dummy(ImVec2(1.f, 24.f));
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCircle(ImVec2(p.x + 137.f, p.y + 20.f), 13.f, IM_COL32(235, 235, 245, 120), 24, 1.8f);
            dl->AddLine(ImVec2(p.x + 146.f, p.y + 29.f), ImVec2(p.x + 154.f, p.y + 37.f), IM_COL32(235, 235, 245, 120), 1.8f);
            ImGui::Dummy(ImVec2(1.f, 46.f));
            const char* emptyTitle = "No player data";
            ImVec2 titleSize = ImGui::CalcTextSize(emptyTitle);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (274.f - titleSize.x) * 0.5f);
            ImGui::TextUnformatted(emptyTitle);
            ImGui::TextWrapped("This application does not read player or game data. The list will stay empty until a legitimate data source is added.");
            EndCard(cd, "Nothing is being detected or simulated.");
        }
        ImGui::EndChild();
    } else if (s.tab == 3) {
        ImGui::BeginChild("##miscLeft", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "OVERLAY BEHAVIOR");
            { auto r = Row("Show FPS badge", true); Switch(r, &s.show_fps); }
            { auto r = Row("Compact layout");       Switch(r, &s.compact_mode); }
            { auto r = Row("Smooth animations");    Switch(r, &s.animations_enabled); }
            {
                auto r = Row("Keyboard navigation");
                if (Switch(r, &s.keyboard_navigation)) {
                    ImGui::GetIO().ConfigFlags = s.keyboard_navigation
                        ? (ImGui::GetIO().ConfigFlags | ImGuiConfigFlags_NavEnableKeyboard)
                        : (ImGui::GetIO().ConfigFlags & ~ImGuiConfigFlags_NavEnableKeyboard);
                }
            }
            EndCard(cd, "Settings apply immediately to this interface.");
        }
        ImGui::EndChild();
        ImGui::SameLine(0.f, 17.f);
        ImGui::BeginChild("##miscRight", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "PERFORMANCE");
            ImGui::TextUnformatted("Overlay frame rate");
            ImGui::Text("%0.0f FPS", s.render_fps > 0.f ? s.render_fps : ImGui::GetIO().Framerate);
            ImGui::Dummy(ImVec2(1.f, 12.f));
            ImGui::TextWrapped("The counter measures this overlay's render loop. It is not a reading of an external game's FPS.");
            ImGui::Dummy(ImVec2(1.f, 10.f));
            EndCard(cd, "Frame pacing is capped at 144 updates/s.");
        }
        ImGui::EndChild();
    } else if (s.tab == 4) {
        ImGui::BeginChild("##configLeft", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "LOCAL PROFILE");
            ImGui::TextUnformatted("Profile name");
            ImGui::SetNextItemWidth(266.f);
            ImGui::InputText("##profile_name", s.profile_name, sizeof(s.profile_name));
            ImGui::Dummy(ImVec2(1.f, 8.f));
            if (ImGui::Button("Save in memory", ImVec2(126.f, 30.f))) {
                savedProfile = s;
                hasSavedProfile = true;
            }
            ImGui::SameLine(0.f, 8.f);
            ImGui::BeginDisabled(!hasSavedProfile);
            if (ImGui::Button("Load saved", ImVec2(126.f, 30.f))) {
                s = savedProfile;
                ImGui::GetIO().ConfigFlags = s.keyboard_navigation
                    ? (ImGui::GetIO().ConfigFlags | ImGuiConfigFlags_NavEnableKeyboard)
                    : (ImGui::GetIO().ConfigFlags & ~ImGuiConfigFlags_NavEnableKeyboard);
            }
            ImGui::EndDisabled();
            ImGui::Dummy(ImVec2(1.f, 8.f));
            if (ImGui::Button("Restore defaults", ImVec2(260.f, 30.f))) {
                s = State{};
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            }
            EndCard(cd, "Saved profiles are kept only until the app closes.");
        }
        ImGui::EndChild();
        ImGui::SameLine(0.f, 17.f);
        ImGui::BeginChild("##configRight", ImVec2(306.f, -62.f), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            Card cd; BeginCard(cd, "PROFILE STATUS");
            ImGui::TextUnformatted("Current profile");
            ImGui::Text("%s", s.profile_name[0] ? s.profile_name : "Unnamed");
            ImGui::Dummy(ImVec2(1.f, 12.f));
            ImGui::TextUnformatted("Saved snapshot");
            ImGui::TextColored(hasSavedProfile ? ImVec4(0.19f, 0.82f, 0.38f, 1.f)
                                               : ImVec4(0.65f, 0.65f, 0.69f, 1.f),
                               "%s", hasSavedProfile ? "Available in memory" : "No snapshot saved");
            ImGui::Dummy(ImVec2(1.f, 12.f));
            ImGui::TextWrapped("Profiles are not written to disk yet. Save in memory creates a session-only snapshot.");
            EndCard(cd, "No fake disk save/load operation.");
        }
        ImGui::EndChild();
    }

    ImGui::PopStyleVar();
}

// ------------------------------------------------------------ style + entry
inline void Style() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.PopupRounding = 10.f;  st.PopupBorderSize = 1.f;
    st.FrameRounding = 8.f;   st.WindowRounding = 14.f;
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text]          = ImVec4(0.96f, 0.96f, 0.97f, 1.f);
    c[ImGuiCol_PopupBg]       = ImVec4(0.11f, 0.11f, 0.14f, 0.96f);
    c[ImGuiCol_Border]        = ImVec4(1.f, 1.f, 1.f, 0.14f);
    c[ImGuiCol_Header]        = ImVec4(0.04f, 0.52f, 1.f, 0.55f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.04f, 0.52f, 1.f, 0.75f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.04f, 0.52f, 1.f, 0.90f);
    c[ImGuiCol_FrameBg]       = ImVec4(1.f, 1.f, 1.f, 0.11f);
    c[ImGuiCol_FrameBgHovered]= ImVec4(1.f, 1.f, 1.f, 0.16f);
    c[ImGuiCol_FrameBgActive] = ImVec4(1.f, 1.f, 1.f, 0.20f);
}

inline void Render(State& s, bool* open = nullptr) {
    const ImVec2 size(880.f, 560.f);
    ImGuiIO& io = ImGui::GetIO();
    g_animationsEnabled = s.animations_enabled;
    g_rowHeight = s.compact_mode ? 32.f : ROW_H;

    // Preserve the user's panel position; initialize it to screen center once.
    const float maxPanelX = ImMax(0.f, io.DisplaySize.x - size.x);
    const float maxPanelY = ImMax(0.f, io.DisplaySize.y - size.y);
    if (s.panel_x < 0.f || s.panel_y < 0.f) {
        s.panel_x = maxPanelX * 0.5f;
        s.panel_y = maxPanelY * 0.5f;
    }
    s.panel_x = ImClamp(s.panel_x, 0.f, maxPanelX);
    s.panel_y = ImClamp(s.panel_y, 0.f, maxPanelY);

    // Ease the panel into view without resetting its saved position.
    static float menuReveal = 0.f;
    if (!g_animationsEnabled) menuReveal = 1.f;
    else menuReveal = ImMin(1.f, menuReveal + io.DeltaTime * 6.f);
    const float revealEase = menuReveal * menuReveal * (3.f - 2.f * menuReveal);
    const float revealOffset = (1.f - revealEase) * 18.f;
    const ImVec2 panelPos(s.panel_x, s.panel_y + revealOffset);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    const ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("##menu", nullptr, wf);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 actualSize = ImGui::GetWindowSize();
    DrawBackground(dl, wp, actualSize);

    // Drag the panel by its top header area, away from the window buttons.
    const ImVec2 dragMin(wp.x + 205.f, wp.y + 4.f);
    const ImVec2 dragMax(wp.x + actualSize.x - 70.f, wp.y + 49.f);
    if (!s.panel_dragging && ImGui::IsMouseClicked(0) &&
        ImGui::IsMouseHoveringRect(dragMin, dragMax, false) &&
        !ImGui::IsAnyItemHovered()) {
        s.panel_dragging = true;
        s.panel_drag_start_mouse = io.MousePos;
        s.panel_drag_start_x = s.panel_x;
        s.panel_drag_start_y = s.panel_y;
        ImGui::SetWindowFocus();
    }
    if (s.panel_dragging) {
        if (ImGui::IsMouseDown(0)) {
            s.panel_x = ImClamp(s.panel_drag_start_x + io.MousePos.x - s.panel_drag_start_mouse.x,
                                0.f, maxPanelX);
            s.panel_y = ImClamp(s.panel_drag_start_y + io.MousePos.y - s.panel_drag_start_mouse.y,
                                0.f, maxPanelY);
        } else {
            s.panel_dragging = false;
        }
    }

    // ---------------- sidebar
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::BeginChild("##sb", ImVec2(200.f, actualSize.y), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    {
        ImGui::SetCursorPos(ImVec2(11.f, 19.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(34.f, 6.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.f);
        ImGui::SetNextItemWidth(178.f);
        ImGui::InputTextWithHint("##search", "Search", s.search, sizeof s.search);
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImGui::PopStyleVar(2);
        ImVec2 ic(a.x + 16.f, (a.y + b.y) * 0.5f - 1.f);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        cdl->AddCircle(ic, 4.5f, DIM, 0, 1.5f);
        cdl->AddLine(ImVec2(ic.x + 3.5f, ic.y + 3.5f), ImVec2(ic.x + 7.f, ic.y + 7.f), DIM, 1.5f);

        struct T { const char* n; ImU32 c; };
        const T tabs[] = {
            { "Aim",     IM_COL32(10, 132, 255, 255) },
            { "Visuals", IM_COL32(50, 173, 230, 255) },
            { "Players", IM_COL32(191, 90, 242, 255) },
            { "Misc",    IM_COL32(255, 159, 10, 255) },
            { "Config",  IM_COL32(142, 142, 147, 255) },
        };
        int visibleTabs = 0;
        for (int i = 0; i < 5; i++) {
            if (!MatchesSearch(tabs[i].n, s.search)) continue;
            ImGui::SetCursorPos(ImVec2(11.f, 61.f + visibleTabs++ * 32.f));
            ImU32 col = (i == 0 && s.tab == 0) ? IM_COL32(255, 255, 255, 40) : tabs[i].c;
            if (Tab(tabs[i].n, i, col, s.tab == i, 178.f)) s.tab = i;
        }

        if (visibleTabs == 0) { ImGui::SetCursorPos(ImVec2(16.f, 68.f)); ImGui::TextDisabled("No results"); }
        // FPS badge is anchored to the real panel height (not the old hard-coded 560px).
        const float badgeY = wp.y + actualSize.y - 43.f;
        const ImVec2 badgeMin(wp.x + 14.f, badgeY);
        const ImVec2 badgeMax(wp.x + 116.f, badgeY + 30.f);
        cdl->AddRectFilled(badgeMin, badgeMax, IM_COL32(255, 255, 255, 18), 9.f);
        cdl->AddRect(badgeMin, badgeMax, IM_COL32(255, 255, 255, 28), 9.f, 0, 1.f);
        if (s.show_fps) {
            const float pulse = g_animationsEnabled
                ? 0.72f + 0.28f * sinf(static_cast<float>(ImGui::GetTime()) * 3.2f)
                : 1.f;
            cdl->AddCircleFilled(ImVec2(wp.x + 29.f, wp.y + actualSize.y - 28.f),
                                 4.5f, IM_COL32(48, 209, 88, static_cast<int>(255.f * pulse)), 20);
            char fps[24];
            const float measuredFps = s.render_fps > 0.f ? s.render_fps : io.Framerate;
            snprintf(fps, sizeof fps, "%.0f FPS", measuredFps);
            cdl->AddText(ImVec2(wp.x + 42.f, wp.y + actualSize.y - 35.f), COL_TEXT, fps);
        }
    }
    ImGui::EndChild();

    // Short, eased slide whenever the selected page changes.
    static int previousTab = -1;
    static float pageProgress = 1.f;
    if (previousTab != s.tab) {
        previousTab = s.tab;
        pageProgress = 0.f;
    }
    if (!g_animationsEnabled) pageProgress = 1.f;
    else pageProgress = ImMin(1.f, pageProgress + io.DeltaTime * 7.f);
    const float pageEase = pageProgress * pageProgress * (3.f - 2.f * pageProgress);
    const float pageOffset = (1.f - pageEase) * 12.f;

    // ---------------- content
    ImGui::SetCursorPos(ImVec2(200.f, 0));
    ImGui::BeginChild("##content", ImVec2(0.f, actualSize.y), 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetWindowPos();
        const char* titles[] = { "Aim", "Visuals", "Players", "Misc", "Config" };
        const char* subs[]   = { "Target selection and assist behavior", "Overlay and rendering options",
                                 "Player list and filters", "Miscellaneous options", "Save and load profiles" };

        // Fade the heading while the body slides into place.
        const ImU32 titleColor = IM_COL32(245, 245, 247, static_cast<int>(255.f * pageEase));
        const ImU32 subtitleColor = IM_COL32(235, 235, 245, static_cast<int>(150.f * pageEase));
        cdl->AddText(ImGui::GetFont(), 30.f, ImVec2(cp.x + 25.f, cp.y + 22.f + pageOffset), titleColor, titles[s.tab]);
        cdl->AddText(ImGui::GetFont(), 30.f, ImVec2(cp.x + 25.7f, cp.y + 22.f + pageOffset), titleColor, titles[s.tab]);
        cdl->AddText(ImGui::GetFont(), 16.f, ImVec2(cp.x + 25.f, cp.y + 56.f + pageOffset), subtitleColor, subs[s.tab]);

        // window buttons
        struct WB { float x; ImU32 col; bool close; } wbs[] = {
            { actualSize.x - 42.f - 200.f, IM_COL32(255, 189, 46, 255), false },
            { actualSize.x - 22.f - 200.f, IM_COL32(255, 69, 58, 255), true },
        };
        for (auto& b : wbs) {
            ImVec2 ctr(cp.x + b.x, cp.y + 21.f);
            ImGui::SetCursorScreenPos(ImVec2(ctr.x - 9.f, ctr.y - 9.f));
            ImGui::PushID(b.close);
            bool hit = ImGui::InvisibleButton("wb", ImVec2(18.f, 18.f));
            ImGui::PopID();
            cdl->AddCircleFilled(ctr, 8.f, b.col, 20);
            if (b.close) {
                cdl->AddLine(ImVec2(ctr.x - 3.f, ctr.y - 3.f), ImVec2(ctr.x + 3.f, ctr.y + 3.f), IM_COL32_WHITE, 1.8f);
                cdl->AddLine(ImVec2(ctr.x + 3.f, ctr.y - 3.f), ImVec2(ctr.x - 3.f, ctr.y + 3.f), IM_COL32_WHITE, 1.8f);
                if (hit && open) *open = false;
            } else {
                if (hit) s.request_minimize = true;
                cdl->AddLine(ImVec2(ctr.x - 3.5f, ctr.y), ImVec2(ctr.x + 3.5f, ctr.y), IM_COL32_WHITE, 1.8f);
            }
        }

        s.tab = ImClamp(s.tab, 0, 4);
        if (s.tab == 0) AimPage(s, pageOffset);
        else SecondaryPage(s, pageOffset);

        // help button
        ImVec2 hc(cp.x + 25.f + 11.f, cp.y + actualSize.y - 38.f);
        ImGui::SetCursorScreenPos(ImVec2(hc.x - 14.f, hc.y - 14.f));
        if (ImGui::InvisibleButton("help_button", ImVec2(28.f, 28.f))) s.show_help = !s.show_help;
        cdl->AddCircleFilled(hc, 14.f, IM_COL32(120, 120, 128, 140), 24);
        ImVec2 ts = ImGui::CalcTextSize("?");
        cdl->AddText(ImVec2(hc.x - ts.x * 0.5f, hc.y - ts.y * 0.5f), COL_TEXT, "?");
        if (s.show_help) { ImGui::SetCursorPos(ImVec2(25.f, actualSize.y - 105.f)); ImGui::TextWrapped("Demo UI: use the sidebar to navigate. Settings are local to this session."); }
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(4);
}

} // namespace menu
