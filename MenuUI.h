#pragma once

#include "imgui.h"
#include <cstdint>

namespace menu_ui {

enum class Tab : int {
    Aimbot = 0,
    Visuals = 1,
    Settings = 2,
    Count = 3
};

enum class ThemeType : int {
    Rose = 0,
    Forest = 1,
    Mocha = 2,
    Count = 3
};

enum class BackdropType : int {
    Lightning = 0,
    Plasma = 1,
    Galaxy = 2,
    None = 3,
    Count = 4
};

struct MenuState {
    // Window & Navigation
    Tab currentTab = Tab::Settings;
    bool menuOpen = true;
    bool exitRequested = false;
    bool exitCompleted = false;

    // Grape Settings Tab (matches uploaded screenshots)
    ThemeType theme = ThemeType::Rose;
    BackdropType backdrop = BackdropType::Lightning;
    float menuScale = 1.27f;
    bool showFps = true;
    bool watermark = false;
    bool animations = false;

    // Aimbot Tab
    bool aimbotEnabled = true;
    bool aimShowFov = true;
    bool aimVisCheck = true;
    bool aimAutoFire = false;
    int aimBone = 0; // 0: Head, 1: Neck, 2: Chest
    float aimFov = 95.0f;
    float aimSmooth = 5.5f;

    // Visuals Tab
    bool espEnabled = true;
    bool espBoxes = true;
    bool espSnaplines = false;
    bool espHealth = true;
    bool espDistance = true;
    int espBoxStyle = 0; // 0: Cornered, 1: Full Box, 2: Rounded
    float espMaxDist = 250.0f;

    // Chams (published to chams.so via ChamsCfg.h)
    bool chamsEnabled = false;
    bool chamsDump = false;
    int chamsColor = 0; // 0: Red, 1: Green, 2: Cyan, 3: Yellow, 4: Magenta, 5: White
    float chamsStrength = 1.0f;

    // System / Touch / Recording flags used by Main.cpp
    bool miscHideRecord = false;
    bool miscTouchGrab = true;
    ImVec4 activeTouchRect = ImVec4(0, 0, 0, 0);
    bool hideRecordChanged = false;
};

void Init(MenuState& state, float screenW, float screenH);
void LoadFonts(float baseScale = 1.0f);
void SetAvatarTexture(ImTextureID texId);
void PushToast(const char* nameRu, const char* nameEn, bool enabled);
void Render(MenuState& state, float dt);

} // namespace menu_ui
