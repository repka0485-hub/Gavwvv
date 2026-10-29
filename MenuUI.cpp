#include "MenuUI.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace menu_ui {

namespace {

static constexpr float kPi = 3.14159265358979323846f;

static inline float Clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static inline float Lerp(float a, float b, float t) {
    return a + (b - a) * Clamp01(t);
}

static inline ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    t = Clamp01(t);
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

static inline float SmoothDamp(float current, float target, float speed, float dt) {
    float factor = 1.0f - std::exp(-speed * dt);
    return current + (target - current) * Clamp01(factor);
}

static inline ImU32 ToU32(const ImVec4& c, float alphaMul = 1.0f) {
    ImVec4 m = c;
    m.w = Clamp01(m.w * alphaMul);
    return ImGui::ColorConvertFloat4ToU32(m);
}

static inline ImU32 RGBA(int r, int g, int b, int a = 255, float alphaMul = 1.0f) {
    int fa = (int)std::round((float)a * Clamp01(alphaMul));
    fa = std::clamp(fa, 0, 255);
    return IM_COL32(r, g, b, fa);
}

// Clamp any 2D point inside [rMin, rMax] so it never pokes outside the rounded corners of radius `rad`
static inline ImVec2 ClampToRoundedRect(ImVec2 p, const ImVec2& rMin, const ImVec2& rMax, float rad) {
    p.x = std::clamp(p.x, rMin.x, rMax.x);
    p.y = std::clamp(p.y, rMin.y, rMax.y);
    if (rad <= 0.5f) return p;

    float cx = 0.0f, cy = 0.0f;
    bool inCorner = false;
    if (p.x < rMin.x + rad && p.y < rMin.y + rad) {
        cx = rMin.x + rad; cy = rMin.y + rad; inCorner = true;
    } else if (p.x > rMax.x - rad && p.y < rMin.y + rad) {
        cx = rMax.x - rad; cy = rMin.y + rad; inCorner = true;
    } else if (p.x < rMin.x + rad && p.y > rMax.y - rad) {
        cx = rMin.x + rad; cy = rMax.y - rad; inCorner = true;
    } else if (p.x > rMax.x - rad && p.y > rMax.y - rad) {
        cx = rMax.x - rad; cy = rMax.y - rad; inCorner = true;
    }

    if (inCorner) {
        float dx = p.x - cx;
        float dy = p.y - cy;
        float d2 = dx * dx + dy * dy;
        if (d2 > rad * rad && d2 > 1e-6f) {
            float invD = rad / std::sqrt(d2);
            p.x = cx + dx * invD;
            p.y = cy + dy * invD;
        }
    }
    return p;
}

struct ThemeColors {
    ImVec4 winBg;         // Base window fill
    ImVec4 sidebarStrip;  // Left sidebar vertical strip
    ImVec4 activePill;    // Selected sidebar tab rounded square
    ImVec4 activeIcon;    // Selected sidebar tab icon tint
    ImVec4 inactiveIcon;  // Unselected sidebar tab icon tint
    ImVec4 accent;        // Slider active track, thumb, checked checkbox fill
    ImVec4 comboFill;     // Combo box & unchecked checkbox translucent fill
    ImVec4 comboBorder;   // Combo box & unchecked checkbox subtle border
    ImVec4 glowTint;      // Accent glow for Lightning / effects
};

static ThemeColors GetThemePalette(ThemeType t) {
    ThemeColors c{};
    switch (t) {
        case ThemeType::Rose:
        default:
            c.winBg        = ImVec4(0.353f, 0.255f, 0.306f, 0.96f); // #5A414E
            c.sidebarStrip = ImVec4(0.290f, 0.212f, 0.255f, 0.32f); // Subtle darker rose strip
            c.activePill   = ImVec4(0.569f, 0.439f, 0.482f, 0.88f); // #91707B
            c.activeIcon   = ImVec4(0.965f, 0.847f, 0.878f, 1.00f); // #F6D8E0
            c.inactiveIcon = ImVec4(0.910f, 0.855f, 0.882f, 0.92f); // #E8DAE1
            c.accent       = ImVec4(0.918f, 0.702f, 0.784f, 1.00f); // #EAB3C8
            c.comboFill    = ImVec4(0.490f, 0.412f, 0.455f, 0.62f); // #7D6974
            c.comboBorder  = ImVec4(0.840f, 0.775f, 0.810f, 0.48f);
            c.glowTint     = ImVec4(0.945f, 0.725f, 0.815f, 1.00f);
            break;

        case ThemeType::Forest:
            c.winBg        = ImVec4(0.255f, 0.345f, 0.318f, 0.95f); // Deep forest sage
            c.sidebarStrip = ImVec4(0.180f, 0.255f, 0.235f, 0.26f);
            c.activePill   = ImVec4(0.680f, 0.745f, 0.635f, 0.55f); // #ADC0A2 sage pill
            c.activeIcon   = ImVec4(0.955f, 0.975f, 0.930f, 1.00f);
            c.inactiveIcon = ImVec4(0.895f, 0.925f, 0.890f, 0.92f);
            c.accent       = ImVec4(0.859f, 0.898f, 0.769f, 1.00f); // #DBE5C4 pastel sage
            c.comboFill    = ImVec4(0.530f, 0.575f, 0.585f, 0.58f); // #879395
            c.comboBorder  = ImVec4(0.850f, 0.890f, 0.855f, 0.46f);
            c.glowTint     = ImVec4(0.820f, 0.910f, 0.750f, 1.00f);
            break;

        case ThemeType::Mocha:
            c.winBg        = ImVec4(0.282f, 0.247f, 0.420f, 0.96f); // #483F6B cosmic mocha-violet
            c.sidebarStrip = ImVec4(0.200f, 0.175f, 0.310f, 0.28f);
            c.activePill   = ImVec4(0.620f, 0.565f, 0.725f, 0.56f); // #9E90B9 lavender pill
            c.activeIcon   = ImVec4(0.955f, 0.935f, 0.995f, 1.00f);
            c.inactiveIcon = ImVec4(0.885f, 0.865f, 0.945f, 0.92f);
            c.accent       = ImVec4(0.875f, 0.820f, 0.985f, 1.00f); // #DFD1FB pastel lavender
            c.comboFill    = ImVec4(0.510f, 0.485f, 0.585f, 0.58f); // #827C95
            c.comboBorder  = ImVec4(0.835f, 0.810f, 0.910f, 0.46f);
            c.glowTint     = ImVec4(0.855f, 0.785f, 0.985f, 1.00f);
            break;
    }
    return c;
}

struct AnimContext {
    bool initialized = false;
    float baseScale = 1.0f;
    float time = 0.0f;

    // Window position & size
    ImVec2 winPos = ImVec2(140.0f, 60.0f);
    ImVec2 winTargetPos = ImVec2(140.0f, 60.0f);
    bool draggingWin = false;
    ImVec2 dragOffset = ImVec2(0.0f, 0.0f);

    // Floating toggle button
    ImVec2 floatBtnPos = ImVec2(18.0f, 18.0f);
    ImVec2 floatBtnTargetPos = ImVec2(18.0f, 18.0f);
    bool draggingFloatBtn = false;
    bool floatBtnMoved = false;
    ImVec2 floatBtnDragStart = ImVec2(0.0f, 0.0f);
    ImVec2 floatBtnDragOffset = ImVec2(0.0f, 0.0f);
    float floatBtnPulse = 0.0f;

    // Open / Close animation
    float openProgress = 1.0f;

    // Smooth scale lerping
    float smoothMenuScale = 1.27f;

    // Theme blend transition
    ThemeColors curTheme{};

    // Sidebar pill & tab fade animation
    float sidebarPillSlot = 2.0f; // Starts on Settings (slot 2)
    int displayedTab = (int)Tab::Settings;
    float tabContentAlpha = 1.0f;

    // Open dropdown popup state (-1 = none, 0 = Theme/Bone/BoxStyle, 1 = Backdrop)
    int openComboId = -1;
    float comboOpenAnim[3] = {0.0f, 0.0f, 0.0f};
    ImVec2 activeComboPos = ImVec2(0.0f, 0.0f);
    ImVec2 activeComboSize = ImVec2(0.0f, 0.0f);
    const char* const* activeComboItems = nullptr;
    int activeComboCount = 0;
    int* activeComboValue = nullptr;

    // Checkbox & slider animations
    float checkAnim[14] = {};
    int activeSliderId = -1;

    // Smoothed FPS counter
    float smoothFps = 120.0f;
};

static AnimContext g_Anim;
static ImFont* g_FontRegular = nullptr;
static ImFont* g_FontBold = nullptr;
static ImTextureID g_AvatarTex = (ImTextureID)0;

// ============================================================================
// Vector Icon Drawing Helpers (Sidebar Target, Eye, Gear + Close Cross + Check)
// ============================================================================

static void DrawIconTarget(ImDrawList* dl, ImVec2 center, float s, ImU32 col) {
    float rOuter = 6.2f * s;
    float rInner = 1.8f * s;
    float tickIn = 3.8f * s;
    float tickOut = 8.8f * s;
    float thick = std::max(1.4f, 1.75f * s);

    dl->AddCircle(center, rOuter, col, 28, thick);
    dl->AddCircleFilled(center, rInner, col, 16);

    // 4 crosshair ticks (top, bottom, left, right)
    dl->AddLine(ImVec2(center.x, center.y - tickOut), ImVec2(center.x, center.y - tickIn), col, thick);
    dl->AddLine(ImVec2(center.x, center.y + tickIn),  ImVec2(center.x, center.y + tickOut), col, thick);
    dl->AddLine(ImVec2(center.x - tickOut, center.y), ImVec2(center.x - tickIn, center.y), col, thick);
    dl->AddLine(ImVec2(center.x + tickIn, center.y),  ImVec2(center.x + tickOut, center.y), col, thick);
}

static void DrawIconEye(ImDrawList* dl, ImVec2 center, float s, ImU32 col, ImU32 cutoutCol) {
    float rx = 8.4f * s;
    float ry = 5.3f * s;
    float thick = std::max(1.6f, 2.05f * s);

    // Almond eye outline using cubic bezier curves
    ImVec2 left(center.x - rx, center.y);
    ImVec2 right(center.x + rx, center.y);
    dl->PathClear();
    dl->PathLineTo(left);
    dl->PathBezierCubicCurveTo(
        ImVec2(center.x - rx * 0.45f, center.y - ry * 1.35f),
        ImVec2(center.x + rx * 0.45f, center.y - ry * 1.35f),
        right, 16);
    dl->PathBezierCubicCurveTo(
        ImVec2(center.x + rx * 0.45f, center.y + ry * 1.35f),
        ImVec2(center.x - rx * 0.45f, center.y + ry * 1.35f),
        left, 16);
    dl->PathStroke(col, ImDrawFlags_Closed, thick);

    // Pupil + tiny glint cutout
    dl->AddCircleFilled(center, 3.1f * s, col, 20);
    dl->AddCircleFilled(ImVec2(center.x - 0.95f * s, center.y - 0.95f * s), 1.05f * s, cutoutCol, 12);
}

static void DrawIconGear(ImDrawList* dl, ImVec2 center, float s, ImU32 col, ImU32 holeCol) {
    // Solid 8-toothed cog/gear matching the Grape screenshots
    const int numTeeth = 8;
    float rRoot = 5.6f * s;
    float rTip  = 8.2f * s;

    for (int i = 0; i < numTeeth; ++i) {
        float a = (float)i * (2.0f * kPi / (float)numTeeth);
        float daRoot = 0.21f;
        float daTip  = 0.14f;
        ImVec2 p0(center.x + std::cos(a - daRoot) * (rRoot - 0.6f * s),
                  center.y + std::sin(a - daRoot) * (rRoot - 0.6f * s));
        ImVec2 p1(center.x + std::cos(a - daTip) * rTip,
                  center.y + std::sin(a - daTip) * rTip);
        ImVec2 p2(center.x + std::cos(a + daTip) * rTip,
                  center.y + std::sin(a + daTip) * rTip);
        ImVec2 p3(center.x + std::cos(a + daRoot) * (rRoot - 0.6f * s),
                  center.y + std::sin(a + daRoot) * (rRoot - 0.6f * s));
        dl->AddQuadFilled(p0, p1, p2, p3, col);
    }

    dl->AddCircleFilled(center, rRoot, col, 28);
    dl->AddCircleFilled(center, 2.5f * s, holeCol, 20);
}

// ============================================================================
// Procedural Animated Backdrops (Lightning, Plasma, Galaxy)
// ============================================================================

static inline float EvalLightningWave(float v, float t) {
    return 0.51f
        + 0.056f * std::sin(v * 18.5f - t * 2.1f)
        + 0.045f * std::sin(v * 39.0f + t * 1.4f)
        + 0.022f * std::cos(v * 63.0f - t * 2.8f);
}

static ImVec4 EvalPlasmaColor(float u, float v, float t) {
    // 2D multi-frequency interference plasma matching Screenshot 2 (Forest + Plasma)
    float x = u * 2.0f - 1.0f;
    float y = v * 2.0f - 1.0f;

    float v1 = std::sin(x * 2.7f + y * 2.0f + t * 0.55f);
    float v2 = std::cos(x * 2.2f - y * 2.9f - t * 0.45f);
    float v3 = std::sin((x * 1.6f + y * 2.3f) * 1.5f + t * 0.35f);

    // Focal metaball centers matching Screenshot 2's royal-blue & crimson-magenta blobs
    float b1x = -0.16f + 0.12f * std::sin(t * 0.6f);
    float b1y =  0.04f + 0.10f * std::cos(t * 0.5f);
    float b2x =  0.22f + 0.12f * std::cos(t * 0.55f);
    float b2y =  0.43f + 0.10f * std::sin(t * 0.65f);
    float b3x =  0.54f + 0.09f * std::sin(t * 0.4f);
    float b3y = -0.82f + 0.09f * std::cos(t * 0.5f);
    float b4x = -0.58f + 0.09f * std::cos(t * 0.45f);
    float b4y = -0.32f + 0.10f * std::sin(t * 0.5f);

    float dBlue1 = std::exp(-((x - b1x)*(x - b1x) + (y - b1y)*(y - b1y)) * 5.6f);
    float dBlue2 = std::exp(-((x - b3x)*(x - b3x) + (y - b3y)*(y - b3y)) * 6.0f);
    float dMag1  = std::exp(-((x - b2x)*(x - b2x) + (y - b2y)*(y - b2y)) * 5.2f);
    float dMag2  = std::exp(-((x - b4x)*(x - b4x) + (y - b4y)*(y - b4y)) * 5.8f);

    // Cyan halo rings around the blue blobs
    float r1 = std::sqrt((x - b1x)*(x - b1x) + (y - b1y)*(y - b1y));
    float cyanRing = std::exp(-(r1 - 0.46f)*(r1 - 0.46f) * 11.0f);

    float field = (v1 + v2 + v3) * 0.333f;

    ImVec4 colCyan(0.205f, 0.635f, 0.785f, 0.96f);   // #34A2C8 vivid sky-cyan
    ImVec4 colOlive(0.615f, 0.595f, 0.385f, 0.96f);  // #9D9862 warm golden-olive
    ImVec4 colBlue(0.215f, 0.205f, 0.815f, 0.96f);   // #3734D0 royal indigo-blue
    ImVec4 colPurple(0.475f, 0.210f, 0.725f, 0.96f); // #7936B9 inner violet core
    ImVec4 colMagenta(0.735f, 0.175f, 0.455f, 0.96f);// #BC2D74 crimson-magenta

    float cyanFactor = Clamp01(0.48f + 0.52f * std::sin(field * 2.8f + x * 1.5f - y * 1.2f) + cyanRing * 0.45f);
    ImVec4 c = LerpColor(colOlive, colCyan, cyanFactor);

    float blueW = Clamp01(dBlue1 * 1.32f + dBlue2 * 1.20f);
    c = LerpColor(c, colBlue, blueW);
    c = LerpColor(c, colPurple, Clamp01((dBlue1 * dBlue1 + dBlue2 * dBlue2) * 0.92f));

    float magW = Clamp01(dMag1 * 1.30f + dMag2 * 1.00f);
    c = LerpColor(c, colMagenta, magW);

    c.w = 0.96f;
    return c;
}

static inline float Hash21(int ix, int iy) {
    uint32_t n = (uint32_t)(ix * 374761393 + iy * 668265263);
    n = (n ^ (n >> 13)) * 1274126177u;
    return (float)(n & 0x00FFFFFFu) / (float)0x01000000u;
}

static ImVec4 EvalGalaxyNebulaColor(float u, float v, float t) {
    // Sweeping cosmic purple/indigo nebula bands matching Screenshot 3 (Mocha + Galaxy)
    ImVec4 deepSpace(0.235f, 0.220f, 0.375f, 0.96f);   // #3C3860
    ImVec4 midViolet(0.355f, 0.300f, 0.505f, 0.96f);   // #5B4D81
    ImVec4 brightLilac(0.495f, 0.395f, 0.640f, 0.96f); // #7E65A3

    float diag = (u * 0.85f + v * 0.75f) + 0.14f * std::sin(v * 4.5f - u * 3.2f + t * 0.35f);
    float arm1 = 0.5f + 0.5f * std::sin(diag * 7.2f - t * 0.4f);
    float arm2 = std::exp(-((u - 0.44f)*(u - 0.44f) + (v - 0.47f)*(v - 0.47f)) * 6.5f);

    ImVec4 c = LerpColor(deepSpace, midViolet, Clamp01(arm1 * 0.78f + 0.15f));
    c = LerpColor(c, brightLilac, Clamp01(arm2 * 0.78f + arm1 * arm1 * 0.35f));
    c.w = 0.96f;
    return c;
}

static void RenderBackdrop(
    ImDrawList* dl,
    const ImVec2& winPos,
    const ImVec2& winSize,
    float winRounding,
    BackdropType backdrop,
    const ThemeColors& tc,
    float t,
    float masterAlpha)
{
    ImVec2 winMax(winPos.x + winSize.x, winPos.y + winSize.y);

    // 1. Base rounded window fill so the 4 rounded corners are always anti-aliased
    dl->AddRectFilled(winPos, winMax, ToU32(tc.winBg, masterAlpha), winRounding);
    if (backdrop == BackdropType::None) {
        return;
    }

    ImVec2 whiteUv = ImGui::GetIO().Fonts->TexUvWhitePixel;

    if (backdrop == BackdropType::Lightning) {
        // Curve-aligned horizontal multi-stop alpha-blended glow strip following x = EvalLightningWave(v, t)
        // Fades smoothly to alpha = 0 at +-0.095 around the wave so there are zero horizontal streaks
        const int rows = 140;
        const int cols = 6;
        const float offsets[cols + 1] = {
            -0.095f, -0.036f, -0.011f,
             0.0f,
             0.011f,  0.036f,  0.095f
        };

        ImVec4 cZero  = ImVec4(tc.glowTint.x, tc.glowTint.y, tc.glowTint.z, 0.0f);
        ImVec4 cMid   = ImVec4(tc.glowTint.x, tc.glowTint.y, tc.glowTint.z, 0.38f);
        ImVec4 cInner = ImVec4(0.98f, 0.86f, 0.92f, 0.85f);
        ImVec4 cCore  = ImVec4(1.00f, 0.99f, 1.00f, 1.00f);

        const ImU32 stopCols[cols + 1] = {
            ToU32(cZero,  masterAlpha),
            ToU32(cMid,   masterAlpha),
            ToU32(cInner, masterAlpha),
            ToU32(cCore,  masterAlpha),
            ToU32(cInner, masterAlpha),
            ToU32(cMid,   masterAlpha),
            ToU32(cZero,  masterAlpha)
        };

        const int vtxCount = (rows + 1) * (cols + 1);
        const int idxCount = rows * cols * 6;
        dl->PrimReserve(idxCount, vtxCount);
        ImDrawIdx baseIdx = (ImDrawIdx)dl->_VtxCurrentIdx;

        for (int iy = 0; iy <= rows; ++iy) {
            float v = (float)iy / (float)rows;
            float py = winPos.y + v * winSize.y;
            float waveU = EvalLightningWave(v, t);

            for (int ix = 0; ix <= cols; ++ix) {
                float u = std::clamp(waveU + offsets[ix], 0.0f, 1.0f);
                float px = winPos.x + u * winSize.x;
                ImVec2 pos = ClampToRoundedRect(ImVec2(px, py), winPos, winMax, winRounding);
                dl->PrimWriteVtx(pos, whiteUv, stopCols[ix]);
            }
        }

        for (int iy = 0; iy < rows; ++iy) {
            for (int ix = 0; ix < cols; ++ix) {
                ImDrawIdx i0 = (ImDrawIdx)(baseIdx + iy * (cols + 1) + ix);
                ImDrawIdx i1 = (ImDrawIdx)(i0 + 1);
                ImDrawIdx i2 = (ImDrawIdx)(i0 + (cols + 1) + 1);
                ImDrawIdx i3 = (ImDrawIdx)(i0 + (cols + 1));

                dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i1); dl->PrimWriteIdx(i2);
                dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i2); dl->PrimWriteIdx(i3);
            }
        }
        return;
    }

    // 2. Render smooth Gouraud-shaded 2D grid for Plasma & Galaxy
    const int gridX = 34;
    const int gridY = 38;
    const int vtxCount = (gridX + 1) * (gridY + 1);
    const int idxCount = gridX * gridY * 6;
    dl->PrimReserve(idxCount, vtxCount);
    ImDrawIdx baseIdx = (ImDrawIdx)dl->_VtxCurrentIdx;

    for (int iy = 0; iy <= gridY; ++iy) {
        float v = (float)iy / (float)gridY;
        float py = winPos.y + v * winSize.y;
        for (int ix = 0; ix <= gridX; ++ix) {
            float u = (float)ix / (float)gridX;
            float px = winPos.x + u * winSize.x;

            ImVec2 pos = ClampToRoundedRect(ImVec2(px, py), winPos, winMax, winRounding);
            ImVec4 col = (backdrop == BackdropType::Plasma)
                ? EvalPlasmaColor(u, v, t)
                : EvalGalaxyNebulaColor(u, v, t);
            dl->PrimWriteVtx(pos, whiteUv, ToU32(col, masterAlpha));
        }
    }

    for (int iy = 0; iy < gridY; ++iy) {
        for (int ix = 0; ix < gridX; ++ix) {
            ImDrawIdx i0 = (ImDrawIdx)(baseIdx + iy * (gridX + 1) + ix);
            ImDrawIdx i1 = (ImDrawIdx)(i0 + 1);
            ImDrawIdx i2 = (ImDrawIdx)(i0 + (gridX + 1) + 1);
            ImDrawIdx i3 = (ImDrawIdx)(i0 + (gridX + 1));

            dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i1); dl->PrimWriteIdx(i2);
            dl->PrimWriteIdx(i0); dl->PrimWriteIdx(i2); dl->PrimWriteIdx(i3);
        }
    }

    // 3. Multi-scale soft square cell + circular star core pass for Galaxy (matches Screenshot 3)
    if (backdrop == BackdropType::Galaxy) {
        struct StarLayer { int cols; int rows; float density; float sizeScale; float alpha; };
        const StarLayer layers[3] = {
            { 22, 25, 0.22f, 0.92f, 0.30f },
            { 36, 40, 0.19f, 0.80f, 0.48f },
            { 56, 62, 0.13f, 0.68f, 0.72f }
        };

        for (int L = 0; L < 3; ++L) {
            int cols = layers[L].cols;
            int rows = layers[L].rows;
            float cellW = winSize.x / (float)cols;
            float cellH = winSize.y / (float)rows;

            for (int iy = 0; iy < rows; ++iy) {
                for (int ix = 0; ix < cols; ++ix) {
                    float h = Hash21(ix + L * 131, iy + L * 257);
                    if (h > layers[L].density) continue;

                    float phase = Hash21(ix + 71, iy + 193) * 6.283f;
                    float twinkle = 0.60f + 0.40f * std::sin(t * (1.4f + L * 0.6f) + phase);
                    float a = layers[L].alpha * twinkle * masterAlpha;

                    float cx = winPos.x + (ix + 0.5f) * cellW;
                    float cy = winPos.y + (iy + 0.5f) * cellH;
                    float halfW = cellW * 0.42f * layers[L].sizeScale;
                    float halfH = cellH * 0.42f * layers[L].sizeScale;

                    ImVec2 pMin = ClampToRoundedRect(ImVec2(cx - halfW, cy - halfH), winPos, winMax, winRounding);
                    ImVec2 pMax = ClampToRoundedRect(ImVec2(cx + halfW, cy + halfH), winPos, winMax, winRounding);

                    // Faint square grid cell aura + soft radial circular star center
                    dl->AddRectFilled(pMin, pMax, RGBA(210, 195, 245, (int)(a * 82.0f)), 1.5f);
                    float rDot = std::min(halfW, halfH) * 0.46f;
                    ImVec2 cClamped = ClampToRoundedRect(ImVec2(cx, cy), winPos, winMax, winRounding);
                    dl->AddCircleFilled(cClamped, rDot * 1.4f, RGBA(230, 218, 255, (int)(a * 110.0f)), 10);
                    dl->AddCircleFilled(cClamped, rDot * 0.72f, RGBA(252, 246, 255, (int)(a * 225.0f)), 10);
                }
            }
        }
    }
}

// ============================================================================
// Grape Custom UI Widgets (Combo Box, Slider, Rounded Checkbox)
// ============================================================================

static void DrawGrapeComboTrigger(
    ImDrawList* dl,
    int comboId,
    const char* label,
    const char* const* items,
    int itemCount,
    int* currentItem,
    const ImVec2& pos,
    const ImVec2& size,
    float s,
    const ThemeColors& tc,
    float alpha,
    float dt)
{
    ImGuiIO& io = ImGui::GetIO();

    // Label above combo box
    float labelOffsetY = 17.5f * s;
    dl->AddText(ImVec2(pos.x + 2.0f * s, pos.y - labelOffsetY), RGBA(246, 242, 250, 248, alpha), label);

    ImVec2 bMin = pos;
    ImVec2 bMax(pos.x + size.x, pos.y + size.y);
    float rounding = size.y * 0.28f;

    bool hovered = (g_Anim.openComboId == -1) &&
                   io.MousePos.x >= bMin.x && io.MousePos.x <= bMax.x &&
                   io.MousePos.y >= bMin.y && io.MousePos.y <= bMax.y;

    if (hovered && io.MouseClicked[0]) {
        g_Anim.openComboId = comboId;
        g_Anim.activeComboPos = bMin;
        g_Anim.activeComboSize = size;
        g_Anim.activeComboItems = items;
        g_Anim.activeComboCount = itemCount;
        g_Anim.activeComboValue = currentItem;
    }

    bool isOpen = (g_Anim.openComboId == comboId);
    g_Anim.comboOpenAnim[comboId] = SmoothDamp(g_Anim.comboOpenAnim[comboId], isOpen ? 1.0f : 0.0f, 18.0f, dt);

    ImVec4 fillCol = tc.comboFill;
    if (hovered || isOpen) {
        fillCol.x = Clamp01(fillCol.x + 0.05f);
        fillCol.y = Clamp01(fillCol.y + 0.05f);
        fillCol.z = Clamp01(fillCol.z + 0.05f);
    }

    dl->AddRectFilled(bMin, bMax, ToU32(fillCol, alpha), rounding);
    dl->AddRect(bMin, bMax, ToU32(tc.comboBorder, alpha), rounding, 0, std::max(1.0f, 1.15f * s));

    // Selected option text
    int idx = std::clamp(*currentItem, 0, itemCount - 1);
    const char* curText = items[idx];
    ImVec2 ts = ImGui::CalcTextSize(curText);
    dl->AddText(
        ImVec2(bMin.x + 11.0f * s, bMin.y + (size.y - ts.y) * 0.5f),
        RGBA(246, 242, 250, 248, alpha),
        curText);

    // Downward chevron 'v' on the right
    float chevX = bMax.x - 14.0f * s;
    float chevY = bMin.y + size.y * 0.5f;
    float cw = 4.2f * s;
    float ch = 2.4f * s * (1.0f - 2.0f * g_Anim.comboOpenAnim[comboId]);
    ImU32 chevCol = RGBA(242, 238, 248, 235, alpha);
    float chevThick = std::max(1.2f, 1.55f * s);
    dl->AddLine(ImVec2(chevX - cw, chevY - ch), ImVec2(chevX, chevY + ch), chevCol, chevThick);
    dl->AddLine(ImVec2(chevX, chevY + ch), ImVec2(chevX + cw, chevY - ch), chevCol, chevThick);
}

static void RenderOpenComboPopup(ImDrawList* dl, float s, const ThemeColors& tc, float alpha) {
    if (g_Anim.openComboId < 0 || !g_Anim.activeComboItems || !g_Anim.activeComboValue) return;

    int id = g_Anim.openComboId;
    float openT = g_Anim.comboOpenAnim[id];
    if (openT < 0.02f) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 bMin = g_Anim.activeComboPos;
    ImVec2 size = g_Anim.activeComboSize;
    int count = g_Anim.activeComboCount;

    float itemH = size.y * 0.94f;
    float popH = itemH * (float)count * openT;
    ImVec2 pMin(bMin.x, bMin.y + size.y + 4.0f * s);
    ImVec2 pMax(bMin.x + size.x, pMin.y + popH);
    float rounding = size.y * 0.26f;

    // Darkened theme popup card
    ImVec4 popBg = tc.winBg;
    popBg.x = Clamp01(popBg.x * 0.85f);
    popBg.y = Clamp01(popBg.y * 0.85f);
    popBg.z = Clamp01(popBg.z * 0.85f);
    popBg.w = 0.98f;

    dl->AddRectFilled(
        ImVec2(pMin.x, pMin.y + 3.0f * s),
        ImVec2(pMax.x, pMax.y + 3.0f * s),
        RGBA(0, 0, 0, 95, alpha * openT),
        rounding);
    dl->AddRectFilled(pMin, pMax, ToU32(popBg, alpha * openT), rounding);
    dl->AddRect(pMin, pMax, ToU32(tc.comboBorder, alpha * openT), rounding, 0, std::max(1.0f, 1.15f * s));

    bool clickedInsidePopup = false;
    for (int i = 0; i < count; ++i) {
        ImVec2 rMin(pMin.x + 3.0f * s, pMin.y + i * itemH + 2.0f * s);
        ImVec2 rMax(pMax.x - 3.0f * s, pMin.y + (i + 1) * itemH - 2.0f * s);

        bool hov = io.MousePos.x >= rMin.x && io.MousePos.x <= rMax.x &&
                   io.MousePos.y >= rMin.y && io.MousePos.y <= rMax.y;
        bool selected = (*g_Anim.activeComboValue == i);

        if (selected || hov) {
            ImVec4 hl = selected ? tc.activePill : tc.comboFill;
            dl->AddRectFilled(rMin, rMax, ToU32(hl, alpha * openT * (selected ? 0.85f : 0.55f)), rounding * 0.75f);
        }

        ImVec2 ts = ImGui::CalcTextSize(g_Anim.activeComboItems[i]);
        dl->AddText(
            ImVec2(pMin.x + 11.0f * s, pMin.y + i * itemH + (itemH - ts.y) * 0.5f),
            RGBA(248, 244, 252, 248, alpha * openT),
            g_Anim.activeComboItems[i]);

        if (hov && io.MouseClicked[0]) {
            *g_Anim.activeComboValue = i;
            g_Anim.openComboId = -1;
            clickedInsidePopup = true;
        }
    }

    // Close popup when clicking anywhere outside trigger + popup
    bool inTrigger = io.MousePos.x >= bMin.x && io.MousePos.x <= bMin.x + size.x &&
                     io.MousePos.y >= bMin.y && io.MousePos.y <= bMin.y + size.y;
    bool inPopup = io.MousePos.x >= pMin.x && io.MousePos.x <= pMax.x &&
                   io.MousePos.y >= pMin.y && io.MousePos.y <= pMax.y;
    if (io.MouseClicked[0] && !inTrigger && !inPopup && !clickedInsidePopup) {
        g_Anim.openComboId = -1;
    }
}

static bool DrawGrapeSlider(
    ImDrawList* dl,
    int sliderId,
    const char* label,
    float* value,
    float vMin,
    float vMax,
    const char* fmt,
    const ImVec2& pos,
    float width,
    float s,
    const ThemeColors& tc,
    float alpha)
{
    ImGuiIO& io = ImGui::GetIO();

    // Label + value right next to it (e.g. "Menu scale  1.27")
    float labelY = pos.y - 19.5f * s;
    dl->AddText(ImVec2(pos.x + 1.0f * s, labelY), RGBA(246, 242, 250, 248, alpha), label);

    char valBuf[32];
    std::snprintf(valBuf, sizeof(valBuf), fmt, *value);
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    dl->AddText(
        ImVec2(pos.x + labelSize.x + 8.5f * s, labelY),
        RGBA(230, 222, 236, 215, alpha),
        valBuf);

    // Horizontal track geometry
    float trackH = std::max(2.6f, 3.4f * s);
    float thumbR = 6.2f * s;
    ImVec2 hitMin(pos.x - thumbR, pos.y - 10.0f * s);
    ImVec2 hitMax(pos.x + width + thumbR, pos.y + 10.0f * s);

    bool hovered = (g_Anim.openComboId == -1) &&
                   io.MousePos.x >= hitMin.x && io.MousePos.x <= hitMax.x &&
                   io.MousePos.y >= hitMin.y && io.MousePos.y <= hitMax.y;

    if (hovered && io.MouseClicked[0]) {
        g_Anim.activeSliderId = sliderId;
    }
    if (!io.MouseDown[0] && g_Anim.activeSliderId == sliderId) {
        g_Anim.activeSliderId = -1;
    }

    bool changed = false;
    if (g_Anim.activeSliderId == sliderId && io.MouseDown[0]) {
        float t = Clamp01((io.MousePos.x - pos.x) / std::max(1.0f, width));
        float newVal = vMin + t * (vMax - vMin);
        if (std::fabs(newVal - *value) > 1e-4f) {
            *value = newVal;
            changed = true;
        }
    }

    float frac = Clamp01((*value - vMin) / (vMax - vMin));
    float splitX = pos.x + frac * width;

    // Inactive right track
    dl->AddRectFilled(
        ImVec2(pos.x, pos.y - trackH * 0.5f),
        ImVec2(pos.x + width, pos.y + trackH * 0.5f),
        ToU32(tc.comboFill, alpha * 0.90f),
        trackH * 0.5f);

    // Active left track in Theme Accent color
    if (frac > 0.005f) {
        dl->AddRectFilled(
            ImVec2(pos.x, pos.y - trackH * 0.5f),
            ImVec2(splitX, pos.y + trackH * 0.5f),
            ToU32(tc.accent, alpha),
            trackH * 0.5f);
    }

    // Circular thumb handle
    dl->AddCircleFilled(ImVec2(splitX, pos.y), thumbR, ToU32(tc.accent, alpha), 24);
    return changed;
}

static bool DrawGrapeCheckbox(
    ImDrawList* dl,
    int checkIdx,
    const char* label,
    bool* value,
    const ImVec2& pos,
    float s,
    const ThemeColors& tc,
    float alpha,
    float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    float boxSz = 17.5f * s;
    float rounding = 4.6f * s;

    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 hitMin = pos;
    ImVec2 hitMax(pos.x + boxSz + 10.0f * s + ts.x + 8.0f * s, pos.y + boxSz);

    bool hovered = (g_Anim.openComboId == -1) &&
                   io.MousePos.x >= hitMin.x && io.MousePos.x <= hitMax.x &&
                   io.MousePos.y >= hitMin.y && io.MousePos.y <= hitMax.y;

    bool toggled = false;
    if (hovered && io.MouseClicked[0]) {
        *value = !(*value);
        toggled = true;
    }

    float& anim = g_Anim.checkAnim[checkIdx];
    anim = SmoothDamp(anim, *value ? 1.0f : 0.0f, 18.0f, dt);

    ImVec2 bMin = pos;
    ImVec2 bMax(pos.x + boxSz, pos.y + boxSz);

    ImVec4 fillCol = LerpColor(tc.comboFill, tc.accent, anim);
    dl->AddRectFilled(bMin, bMax, ToU32(fillCol, alpha), rounding);

    if (anim < 0.95f) {
        dl->AddRect(bMin, bMax, ToU32(tc.comboBorder, alpha * (1.0f - anim)), rounding, 0, std::max(1.0f, 1.1f * s));
    }

    // Crisp white checkmark when checked
    if (anim > 0.05f) {
        ImU32 checkCol = RGBA(255, 255, 255, 252, alpha * anim);
        float cx = bMin.x + boxSz * 0.5f;
        float cy = bMin.y + boxSz * 0.52f;
        ImVec2 p0(cx - 4.0f * s, cy + 0.1f * s);
        ImVec2 p1(cx - 1.1f * s, cy + 3.0f * s);
        ImVec2 p2(cx + 4.3f * s, cy - 2.9f * s);
        float cThick = std::max(1.5f, 1.95f * s);
        dl->AddLine(p0, p1, checkCol, cThick);
        dl->AddLine(p1, p2, checkCol, cThick);
    }

    dl->AddText(
        ImVec2(bMax.x + 10.5f * s, bMin.y + (boxSz - ts.y) * 0.5f),
        RGBA(246, 242, 250, 248, alpha),
        label);

    return toggled;
}

// ============================================================================
// Floating Open/Close Button & Top HUD (Show FPS / Watermark)
// ============================================================================

static void RenderTopOverlays(ImDrawList* dl, const MenuState& state, float s, const ThemeColors& tc) {
    float xCursor = 74.0f * s;
    float yTop = 14.0f * s;
    float pillH = 24.0f * s;

    if (state.watermark) {
        const char* wmText = "Grape  |  v1.0";
        ImVec2 ts = ImGui::CalcTextSize(wmText);
        float pw = ts.x + 20.0f * s;
        ImVec2 pMin(xCursor, yTop);
        ImVec2 pMax(xCursor + pw, yTop + pillH);

        dl->AddRectFilled(pMin, pMax, ToU32(tc.winBg, 0.90f), pillH * 0.45f);
        dl->AddRect(pMin, pMax, ToU32(tc.accent, 0.65f), pillH * 0.45f, 0, 1.1f * s);
        dl->AddText(ImVec2(pMin.x + 10.0f * s, pMin.y + (pillH - ts.y) * 0.5f), RGBA(248, 244, 252, 245), wmText);
        xCursor += pw + 8.0f * s;
    }

    if (state.showFps) {
        char fpsBuf[32];
        int fpsVal = (int)std::round(g_Anim.smoothFps);
        std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %d", fpsVal);
        ImVec2 ts = ImGui::CalcTextSize(fpsBuf);
        float pw = ts.x + 18.0f * s;
        ImVec2 pMin(xCursor, yTop);
        ImVec2 pMax(xCursor + pw, yTop + pillH);

        dl->AddRectFilled(pMin, pMax, ToU32(tc.winBg, 0.90f), pillH * 0.45f);
        dl->AddRect(pMin, pMax, ToU32(tc.comboBorder, 0.60f), pillH * 0.45f, 0, 1.1f * s);
        dl->AddText(ImVec2(pMin.x + 9.0f * s, pMin.y + (pillH - ts.y) * 0.5f), ToU32(tc.accent, 1.0f), fpsBuf);
    }
}

static void RenderFloatingToggleButton(
    ImDrawList* dl,
    MenuState& state,
    float s,
    float screenW,
    float screenH,
    const ThemeColors& tc,
    float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    float btnSz = 38.0f * s;
    float radius = btnSz * 0.34f;

    g_Anim.floatBtnTargetPos.x = std::clamp(g_Anim.floatBtnTargetPos.x, 8.0f * s, std::max(8.0f * s, screenW - btnSz - 8.0f * s));
    g_Anim.floatBtnTargetPos.y = std::clamp(g_Anim.floatBtnTargetPos.y, 8.0f * s, std::max(8.0f * s, screenH - btnSz - 8.0f * s));
    g_Anim.floatBtnPos.x = SmoothDamp(g_Anim.floatBtnPos.x, g_Anim.floatBtnTargetPos.x, 20.0f, dt);
    g_Anim.floatBtnPos.y = SmoothDamp(g_Anim.floatBtnPos.y, g_Anim.floatBtnTargetPos.y, 20.0f, dt);

    ImVec2 bMin = g_Anim.floatBtnPos;
    ImVec2 bMax(bMin.x + btnSz, bMin.y + btnSz);
    ImVec2 center(bMin.x + btnSz * 0.5f, bMin.y + btnSz * 0.5f);

    bool hovered = io.MousePos.x >= bMin.x && io.MousePos.x <= bMax.x &&
                   io.MousePos.y >= bMin.y && io.MousePos.y <= bMax.y;

    if (hovered && io.MouseClicked[0]) {
        g_Anim.draggingFloatBtn = true;
        g_Anim.floatBtnMoved = false;
        g_Anim.floatBtnDragStart = io.MousePos;
        g_Anim.floatBtnDragOffset = ImVec2(io.MousePos.x - bMin.x, io.MousePos.y - bMin.y);
    }

    if (g_Anim.draggingFloatBtn) {
        if (io.MouseDown[0]) {
            float dx = io.MousePos.x - g_Anim.floatBtnDragStart.x;
            float dy = io.MousePos.y - g_Anim.floatBtnDragStart.y;
            if (dx * dx + dy * dy > (5.0f * s) * (5.0f * s)) {
                g_Anim.floatBtnMoved = true;
            }
            if (g_Anim.floatBtnMoved) {
                g_Anim.floatBtnTargetPos = ImVec2(
                    io.MousePos.x - g_Anim.floatBtnDragOffset.x,
                    io.MousePos.y - g_Anim.floatBtnDragOffset.y);
            }
        } else {
            if (!g_Anim.floatBtnMoved) {
                state.menuOpen = !state.menuOpen;
                g_Anim.floatBtnPulse = 1.0f;
            }
            g_Anim.draggingFloatBtn = false;
        }
    }

    g_Anim.floatBtnPulse = SmoothDamp(g_Anim.floatBtnPulse, 0.0f, 10.0f, dt);

    dl->AddRectFilled(
        ImVec2(bMin.x, bMin.y + 2.0f * s),
        ImVec2(bMax.x, bMax.y + 3.0f * s),
        RGBA(0, 0, 0, 90),
        radius);
    dl->AddRectFilled(bMin, bMax, ToU32(tc.winBg, 0.95f), radius);
    dl->AddRect(bMin, bMax, ToU32(tc.accent, state.menuOpen ? 0.85f : 0.45f), radius, 0, 1.3f * s);

    // Draw mini Grape gear icon inside floating button
    DrawIconGear(dl, center, s * 0.95f, ToU32(tc.accent, 1.0f), ToU32(tc.winBg, 1.0f));
}

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

void LoadFonts(float baseScale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    g_Anim.baseScale = std::clamp(baseScale, 0.55f, 3.0f);
    float pxRegular = std::round(15.0f * g_Anim.baseScale);
    float pxBold    = std::round(16.5f * g_Anim.baseScale);

    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesCyrillic();

    const char* regularCandidates[] = {
        "/system/fonts/Roboto-Medium.ttf",
        "/system/fonts/Roboto-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf"
    };
    const char* boldCandidates[] = {
        "/system/fonts/Roboto-Bold.ttf",
        "/system/fonts/Roboto-Medium.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"
    };

    for (const char* path : regularCandidates) {
        FILE* f = std::fopen(path, "rb");
        if (f) {
            std::fclose(f);
            ImFontConfig cfg;
            cfg.OversampleH = 3;
            cfg.OversampleV = 2;
            g_FontRegular = io.Fonts->AddFontFromFileTTF(path, pxRegular, &cfg, glyphRanges);
            break;
        }
    }

    for (const char* path : boldCandidates) {
        FILE* f = std::fopen(path, "rb");
        if (f) {
            std::fclose(f);
            ImFontConfig cfg;
            cfg.OversampleH = 3;
            cfg.OversampleV = 2;
            g_FontBold = io.Fonts->AddFontFromFileTTF(path, pxBold, &cfg, glyphRanges);
            break;
        }
    }

    if (!g_FontRegular) {
        g_FontRegular = io.Fonts->AddFontDefault();
    }
    if (!g_FontBold) {
        g_FontBold = g_FontRegular;
    }
}

void SetAvatarTexture(ImTextureID texId) {
    g_AvatarTex = texId;
}

void PushToast(const char* nameRu, const char* nameEn, bool enabled) {
    (void)nameRu;
    (void)nameEn;
    (void)enabled;
}

void Init(MenuState& state, float screenW, float screenH) {
    if (ImGui::GetIO().Fonts->Fonts.empty()) {
        float shortSide = std::min(screenW, screenH);
        LoadFonts(shortSide / 720.0f);
    }

    g_Anim.initialized = true;
    g_Anim.smoothMenuScale = state.menuScale;
    g_Anim.curTheme = GetThemePalette(state.theme);
    g_Anim.sidebarPillSlot = (float)state.currentTab;
    g_Anim.displayedTab = (int)state.currentTab;
    g_Anim.openProgress = state.menuOpen ? 1.0f : 0.0f;

    float s = g_Anim.baseScale * (state.menuScale / 1.27f);
    float winW = 390.0f * s;
    float winH = 420.0f * s;

    g_Anim.winPos = ImVec2((screenW - winW) * 0.5f, (screenH - winH) * 0.5f);
    g_Anim.winTargetPos = g_Anim.winPos;
    g_Anim.floatBtnPos = ImVec2(16.0f * g_Anim.baseScale, 14.0f * g_Anim.baseScale);
    g_Anim.floatBtnTargetPos = g_Anim.floatBtnPos;

    // Initialize checkbox animations
    g_Anim.checkAnim[0] = state.showFps ? 1.0f : 0.0f;
    g_Anim.checkAnim[1] = state.watermark ? 1.0f : 0.0f;
    g_Anim.checkAnim[2] = state.animations ? 1.0f : 0.0f;
    g_Anim.checkAnim[3] = state.aimbotEnabled ? 1.0f : 0.0f;
    g_Anim.checkAnim[4] = state.aimShowFov ? 1.0f : 0.0f;
    g_Anim.checkAnim[5] = state.aimVisCheck ? 1.0f : 0.0f;
    g_Anim.checkAnim[6] = state.aimAutoFire ? 1.0f : 0.0f;
    g_Anim.checkAnim[7] = state.espEnabled ? 1.0f : 0.0f;
    g_Anim.checkAnim[8] = state.espBoxes ? 1.0f : 0.0f;
    g_Anim.checkAnim[9] = state.espSnaplines ? 1.0f : 0.0f;
    g_Anim.checkAnim[10] = state.espHealth ? 1.0f : 0.0f;
    g_Anim.checkAnim[11] = state.espDistance ? 1.0f : 0.0f;
    g_Anim.checkAnim[12] = state.chamsEnabled ? 1.0f : 0.0f;
    g_Anim.checkAnim[13] = state.chamsDump ? 1.0f : 0.0f;
}

void Render(MenuState& state, float dt) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : 1280.0f;
    float screenH = io.DisplaySize.y > 0.0f ? io.DisplaySize.y : 720.0f;
    float uiScreenH = std::min(screenW, screenH);

    if (!g_Anim.initialized) {
        Init(state, screenW, uiScreenH);
    }

    dt = std::clamp(dt, 0.001f, 0.05f);
    g_Anim.time += dt;

    float instantFps = 1.0f / std::max(dt, 0.0005f);
    g_Anim.smoothFps = SmoothDamp(g_Anim.smoothFps, std::clamp(instantFps, 30.0f, 120.0f), 4.0f, dt);

    // Smoothly interpolate theme colors when switching between Rose / Forest / Mocha
    ThemeColors targetTheme = GetThemePalette(state.theme);
    float themeLerp = 1.0f - std::exp(-12.0f * dt);
    g_Anim.curTheme.winBg        = LerpColor(g_Anim.curTheme.winBg,        targetTheme.winBg,        themeLerp);
    g_Anim.curTheme.sidebarStrip = LerpColor(g_Anim.curTheme.sidebarStrip, targetTheme.sidebarStrip, themeLerp);
    g_Anim.curTheme.activePill   = LerpColor(g_Anim.curTheme.activePill,   targetTheme.activePill,   themeLerp);
    g_Anim.curTheme.activeIcon   = LerpColor(g_Anim.curTheme.activeIcon,   targetTheme.activeIcon,   themeLerp);
    g_Anim.curTheme.inactiveIcon = LerpColor(g_Anim.curTheme.inactiveIcon, targetTheme.inactiveIcon, themeLerp);
    g_Anim.curTheme.accent       = LerpColor(g_Anim.curTheme.accent,       targetTheme.accent,       themeLerp);
    g_Anim.curTheme.comboFill    = LerpColor(g_Anim.curTheme.comboFill,    targetTheme.comboFill,    themeLerp);
    g_Anim.curTheme.comboBorder  = LerpColor(g_Anim.curTheme.comboBorder,  targetTheme.comboBorder,  themeLerp);
    g_Anim.curTheme.glowTint     = LerpColor(g_Anim.curTheme.glowTint,     targetTheme.glowTint,     themeLerp);

    const ThemeColors& tc = g_Anim.curTheme;

    // Smoothly interpolate Menu scale around reference 1.27
    g_Anim.smoothMenuScale = SmoothDamp(g_Anim.smoothMenuScale, state.menuScale, 14.0f, dt);
    float s = g_Anim.baseScale * (g_Anim.smoothMenuScale / 1.27f);
    float baseS = g_Anim.baseScale;

    ImDrawList* bgList = ImGui::GetBackgroundDrawList();

    // Top HUD overlays (Watermark & Show FPS) + Floating Open/Close Button
    RenderTopOverlays(bgList, state, baseS, tc);
    RenderFloatingToggleButton(bgList, state, baseS, screenW, uiScreenH, tc, dt);

    // Open / Close animation
    float targetOpen = (state.menuOpen && !state.exitRequested) ? 1.0f : 0.0f;
    g_Anim.openProgress = SmoothDamp(g_Anim.openProgress, targetOpen, 14.0f, dt);
    if (state.exitRequested && g_Anim.openProgress < 0.02f) {
        state.exitCompleted = true;
    }

    float floatBtnSz = 38.0f * baseS;
    if (g_Anim.openProgress < 0.01f) {
        state.activeTouchRect = ImVec4(g_Anim.floatBtnPos.x, g_Anim.floatBtnPos.y, floatBtnSz, floatBtnSz);
        return;
    }

    float openEase = 1.0f - (1.0f - g_Anim.openProgress) * (1.0f - g_Anim.openProgress);
    float masterAlpha = Clamp01(g_Anim.openProgress);
    float animScale = s * (0.92f + 0.08f * openEase);

    // Window dimensions matching exact aspect ratio (0.923) of screenshots
    float winW = 392.0f * animScale;
    float winH = 425.0f * animScale;
    float winRounding = 12.0f * animScale;

    // Keep window clamped inside screen bounds
    g_Anim.winTargetPos.x = std::clamp(g_Anim.winTargetPos.x, 6.0f * baseS, std::max(6.0f * baseS, screenW - winW - 6.0f * baseS));
    g_Anim.winTargetPos.y = std::clamp(g_Anim.winTargetPos.y, 6.0f * baseS, std::max(6.0f * baseS, uiScreenH - winH - 6.0f * baseS));
    g_Anim.winPos.x = SmoothDamp(g_Anim.winPos.x, g_Anim.winTargetPos.x, 18.0f, dt);
    g_Anim.winPos.y = SmoothDamp(g_Anim.winPos.y, g_Anim.winTargetPos.y, 18.0f, dt);

    ImVec2 winPos = g_Anim.winPos;
    ImVec2 winSize(winW, winH);
    ImVec2 winMax(winPos.x + winW, winPos.y + winH);

    // Update touch-blocking rectangle
    float minX = std::min(g_Anim.floatBtnPos.x, winPos.x);
    float minY = std::min(g_Anim.floatBtnPos.y, winPos.y);
    float maxX = std::max(g_Anim.floatBtnPos.x + floatBtnSz, winMax.x);
    float maxY = std::max(g_Anim.floatBtnPos.y + floatBtnSz, winMax.y);
    state.activeTouchRect = ImVec4(minX, minY, maxX - minX, maxY - minY);

    // Outer soft drop shadow
    bgList->AddRectFilled(
        ImVec2(winPos.x - 2.0f * animScale, winPos.y + 4.0f * animScale),
        ImVec2(winMax.x + 2.0f * animScale, winMax.y + 6.0f * animScale),
        RGBA(0, 0, 0, 95, masterAlpha),
        winRounding + 2.0f * animScale);

    // 1. Render Procedural Animated Backdrop (Lightning / Plasma / Galaxy / None)
    RenderBackdrop(bgList, winPos, winSize, winRounding, state.backdrop, tc, g_Anim.time, masterAlpha);

    // Subtle outer window border
    bgList->AddRect(winPos, winMax, RGBA(255, 255, 255, 38, masterAlpha), winRounding, 0, 1.0f);

    // 2. Left Sidebar Vertical Strip & 3 Navigation Icons (Target, Eye, Gear)
    float sbInsetX = winW * 0.022f;
    float sbInsetY = winH * 0.018f;
    float sbW      = winW * 0.094f;
    ImVec2 sbMin(winPos.x + sbInsetX, winPos.y + sbInsetY);
    ImVec2 sbMax(sbMin.x + sbW, winMax.y - sbInsetY);
    float sbRounding = sbW * 0.22f;

    bgList->AddRectFilled(sbMin, sbMax, ToU32(tc.sidebarStrip, masterAlpha), sbRounding);

    float sbCenterX = (sbMin.x + sbMax.x) * 0.5f;
    float iconCenterY[3] = {
        winPos.y + winH * 0.068f,
        winPos.y + winH * 0.147f,
        winPos.y + winH * 0.227f
    };
    float pillSize = sbW * 0.78f;
    float pillRounding = pillSize * 0.25f;

    // Smoothly animate active sidebar pill position
    g_Anim.sidebarPillSlot = SmoothDamp(g_Anim.sidebarPillSlot, (float)state.currentTab, 18.0f, dt);
    float pillCenterY = winPos.y + winH * (0.068f + g_Anim.sidebarPillSlot * 0.079f);
    ImVec2 pillMin(sbCenterX - pillSize * 0.5f, pillCenterY - pillSize * 0.5f);
    ImVec2 pillMax(sbCenterX + pillSize * 0.5f, pillCenterY + pillSize * 0.5f);
    bgList->AddRectFilled(pillMin, pillMax, ToU32(tc.activePill, masterAlpha), pillRounding);

    for (int i = 0; i < 3; ++i) {
        ImVec2 c(sbCenterX, iconCenterY[i]);
        ImVec2 hitMin(sbCenterX - pillSize * 0.55f, c.y - pillSize * 0.55f);
        ImVec2 hitMax(sbCenterX + pillSize * 0.55f, c.y + pillSize * 0.55f);

        bool hov = (g_Anim.openComboId == -1) &&
                   io.MousePos.x >= hitMin.x && io.MousePos.x <= hitMax.x &&
                   io.MousePos.y >= hitMin.y && io.MousePos.y <= hitMax.y;
        if (hov && io.MouseClicked[0] && (int)state.currentTab != i) {
            state.currentTab = (Tab)i;
            g_Anim.openComboId = -1;
        }

        bool isSel = ((int)state.currentTab == i);
        ImU32 iconCol = ToU32(isSel ? tc.activeIcon : tc.inactiveIcon, masterAlpha);
        ImU32 holeCol = ToU32(isSel ? tc.activePill : tc.winBg, masterAlpha);

        if (i == 0) {
            DrawIconTarget(bgList, c, animScale * 0.92f, iconCol);
        } else if (i == 1) {
            DrawIconEye(bgList, c, animScale * 0.92f, iconCol, holeCol);
        } else {
            DrawIconGear(bgList, c, animScale * 0.92f, iconCol, holeCol);
        }
    }

    // 3. Header ("Grape" title + "×" close button + window dragging)
    if (g_FontBold) ImGui::PushFont(g_FontBold);
    bgList->AddText(
        ImVec2(winPos.x + winW * 0.146f, winPos.y + winH * 0.028f),
        RGBA(248, 245, 252, 252, masterAlpha),
        "Grape");
    if (g_FontBold) ImGui::PopFont();

    // Top-right '×' close button
    ImVec2 closeCenter(winPos.x + winW * 0.937f, winPos.y + winH * 0.041f);
    float closeHalf = 4.6f * animScale;
    ImVec2 closeHitMin(closeCenter.x - 12.0f * animScale, closeCenter.y - 12.0f * animScale);
    ImVec2 closeHitMax(closeCenter.x + 12.0f * animScale, closeCenter.y + 12.0f * animScale);
    bool closeHov = io.MousePos.x >= closeHitMin.x && io.MousePos.x <= closeHitMax.x &&
                    io.MousePos.y >= closeHitMin.y && io.MousePos.y <= closeHitMax.y;

    ImU32 closeCol = closeHov ? RGBA(255, 255, 255, 255, masterAlpha) : RGBA(242, 238, 248, 235, masterAlpha);
    float closeThick = std::max(1.4f, 1.85f * animScale);
    bgList->AddLine(
        ImVec2(closeCenter.x - closeHalf, closeCenter.y - closeHalf),
        ImVec2(closeCenter.x + closeHalf, closeCenter.y + closeHalf),
        closeCol, closeThick);
    bgList->AddLine(
        ImVec2(closeCenter.x + closeHalf, closeCenter.y - closeHalf),
        ImVec2(closeCenter.x - closeHalf, closeCenter.y + closeHalf),
        closeCol, closeThick);

    if (closeHov && io.MouseClicked[0]) {
        state.menuOpen = false;
        g_Anim.openComboId = -1;
    }

    // Header dragging
    ImVec2 dragHeaderMin(winPos.x + winW * 0.12f, winPos.y);
    ImVec2 dragHeaderMax(closeHitMin.x - 4.0f * animScale, winPos.y + winH * 0.09f);
    bool inHeader = io.MousePos.x >= dragHeaderMin.x && io.MousePos.x <= dragHeaderMax.x &&
                    io.MousePos.y >= dragHeaderMin.y && io.MousePos.y <= dragHeaderMax.y;
    if (inHeader && io.MouseClicked[0] && g_Anim.openComboId == -1) {
        g_Anim.draggingWin = true;
        g_Anim.dragOffset = ImVec2(io.MousePos.x - winPos.x, io.MousePos.y - winPos.y);
    }
    if (g_Anim.draggingWin) {
        if (io.MouseDown[0]) {
            g_Anim.winTargetPos = ImVec2(io.MousePos.x - g_Anim.dragOffset.x, io.MousePos.y - g_Anim.dragOffset.y);
        } else {
            g_Anim.draggingWin = false;
        }
    }

    // 4. Tab Content Area
    if (g_Anim.displayedTab != (int)state.currentTab) {
        g_Anim.tabContentAlpha = SmoothDamp(g_Anim.tabContentAlpha, 0.0f, 24.0f, dt);
        if (g_Anim.tabContentAlpha < 0.08f) {
            g_Anim.displayedTab = (int)state.currentTab;
        }
    } else {
        g_Anim.tabContentAlpha = SmoothDamp(g_Anim.tabContentAlpha, 1.0f, 18.0f, dt);
    }

    float contentAlpha = masterAlpha * Clamp01(g_Anim.tabContentAlpha);
    float contentX = winPos.x + winW * 0.170f;
    float contentW = winW * 0.747f;
    float comboH   = winH * 0.054f;

    if (g_FontRegular) ImGui::PushFont(g_FontRegular);

    if (g_Anim.displayedTab == (int)Tab::Settings) {
        static const char* const kThemeNames[] = { "Rose", "Forest", "Mocha" };
        static const char* const kBackdropNames[] = { "Lightning", "Plasma", "Galaxy", "None" };

        int themeIdx = (int)state.theme;
        DrawGrapeComboTrigger(
            bgList, 0, "Theme", kThemeNames, 3, &themeIdx,
            ImVec2(contentX, winPos.y + winH * 0.135f),
            ImVec2(contentW, comboH),
            animScale, tc, contentAlpha, dt);
        state.theme = (ThemeType)themeIdx;

        int backdropIdx = (int)state.backdrop;
        DrawGrapeComboTrigger(
            bgList, 1, "Backdrop", kBackdropNames, 4, &backdropIdx,
            ImVec2(contentX, winPos.y + winH * 0.233f),
            ImVec2(contentW, comboH),
            animScale, tc, contentAlpha, dt);
        state.backdrop = (BackdropType)backdropIdx;

        DrawGrapeSlider(
            bgList, 0, "Menu scale", &state.menuScale, 0.75f, 1.65f, "%.2f",
            ImVec2(contentX, winPos.y + winH * 0.358f),
            contentW, animScale, tc, contentAlpha);

        DrawGrapeCheckbox(
            bgList, 0, "Show FPS", &state.showFps,
            ImVec2(contentX, winPos.y + winH * 0.402f),
            animScale, tc, contentAlpha, dt);

        DrawGrapeCheckbox(
            bgList, 1, "Watermark", &state.watermark,
            ImVec2(contentX, winPos.y + winH * 0.460f),
            animScale, tc, contentAlpha, dt);

        DrawGrapeCheckbox(
            bgList, 2, "Animations", &state.animations,
            ImVec2(contentX, winPos.y + winH * 0.518f),
            animScale, tc, contentAlpha, dt);
    } else if (g_Anim.displayedTab == (int)Tab::Aimbot) {
        static const char* const kBoneNames[] = { "Head", "Neck", "Chest" };

        DrawGrapeComboTrigger(
            bgList, 0, "Target bone", kBoneNames, 3, &state.aimBone,
            ImVec2(contentX, winPos.y + winH * 0.135f),
            ImVec2(contentW, comboH),
            animScale, tc, contentAlpha, dt);

        DrawGrapeSlider(
            bgList, 1, "FOV radius", &state.aimFov, 20.0f, 240.0f, "%.0f",
            ImVec2(contentX, winPos.y + winH * 0.268f),
            contentW, animScale, tc, contentAlpha);

        DrawGrapeSlider(
            bgList, 2, "Smoothness", &state.aimSmooth, 1.0f, 20.0f, "%.1f",
            ImVec2(contentX, winPos.y + winH * 0.358f),
            contentW, animScale, tc, contentAlpha);

        DrawGrapeCheckbox(
            bgList, 3, "Enable Aimbot", &state.aimbotEnabled,
            ImVec2(contentX, winPos.y + winH * 0.410f),
            animScale, tc, contentAlpha, dt);

        DrawGrapeCheckbox(
            bgList, 4, "Draw FOV circle", &state.aimShowFov,
            ImVec2(contentX, winPos.y + winH * 0.468f),
            animScale, tc, contentAlpha, dt);

        DrawGrapeCheckbox(
            bgList, 5, "Visible check", &state.aimVisCheck,
            ImVec2(contentX, winPos.y + winH * 0.526f),
            animScale, tc, contentAlpha, dt);

        DrawGrapeCheckbox(
            bgList, 6, "Auto fire", &state.aimAutoFire,
            ImVec2(contentX, winPos.y + winH * 0.584f),
            animScale, tc, contentAlpha, dt);
    } else if (g_Anim.displayedTab == (int)Tab::Visuals) {
        static const char* const kChamsColors[] = { "Red", "Green", "Cyan", "Yellow", "Magenta", "White" };

        DrawGrapeComboTrigger(
            bgList, 1, "Hidden color", kChamsColors, 6, &state.chamsColor,
            ImVec2(contentX, winPos.y + winH * 0.135f),
            ImVec2(contentW, comboH),
            animScale, tc, contentAlpha, dt);

        DrawGrapeSlider(
            bgList, 4, "Tint strength", &state.chamsStrength, 0.0f, 1.0f, "%.2f",
            ImVec2(contentX, winPos.y + winH * 0.268f),
            contentW, animScale, tc, contentAlpha);

        DrawGrapeCheckbox(
            bgList, 12, "Chams", &state.chamsEnabled,
            ImVec2(contentX, winPos.y + winH * 0.340f),
            animScale, tc, contentAlpha, dt);

        DrawGrapeCheckbox(
            bgList, 13, "Mesh dump (logcat)", &state.chamsDump,
            ImVec2(contentX, winPos.y + winH * 0.398f),
            animScale, tc, contentAlpha, dt);
    }

    // Render active dropdown popup on top of all widgets
    RenderOpenComboPopup(bgList, animScale, tc, contentAlpha);

    if (g_FontRegular) ImGui::PopFont();
}

} // namespace menu_ui
