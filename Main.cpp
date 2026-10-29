#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "MenuUI.h"
#include "ChamsCfg.h"
#include "AvatarData.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/log.h>
#include "platform/ANativeWindowCreator.h"
#include "platform/TouchHelperA.h"
#endif

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <thread>

#ifdef __ANDROID__

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

static volatile sig_atomic_t g_KeepRunning = 1;

static void SignalHandler(int sig) {
    Touch::EmergencyRelease();
    g_KeepRunning = 0;
    signal(sig, SIG_DFL);
    raise(sig);
}

// When the physical display is currently in portrait (dispW < dispH), rotate all
// ImGui vertices and clip rects 90 degrees clockwise so the UI always displays
// horizontally (landscape: width = maxSide, height = minSide).
static void RotateDrawDataToLandscapeOnPortrait(ImDrawData* drawData, float portraitWidth) {
    if (!drawData) return;
    for (int n = 0; n < drawData->CmdListsCount; ++n) {
        ImDrawList* cmdList = drawData->CmdLists[n];
        ImDrawVert* vtx = cmdList->VtxBuffer.Data;
        for (int i = 0; i < cmdList->VtxBuffer.Size; ++i) {
            float ox = vtx[i].pos.x;
            float oy = vtx[i].pos.y;
            vtx[i].pos.x = portraitWidth - oy;
            vtx[i].pos.y = ox;
        }
        ImDrawCmd* cmd = cmdList->CmdBuffer.Data;
        for (int i = 0; i < cmdList->CmdBuffer.Size; ++i) {
            ImVec4 cr = cmd[i].ClipRect; // (x1, y1, x2, y2)
            float nx1 = std::max(0.0f, portraitWidth - cr.w);
            float ny1 = std::max(0.0f, cr.x);
            float nx2 = std::max(nx1, portraitWidth - cr.y);
            float ny2 = std::max(ny1, cr.z);
            cmd[i].ClipRect = ImVec4(nx1, ny1, nx2, ny2);
        }
    }
}

struct GlContext {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig  config  = nullptr;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    GLuint     avatarTex = 0;

    bool Init(ANativeWindow* window) {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) return false;
        if (!eglInitialize(display, nullptr, nullptr)) return false;

        const EGLint cfgAttribs[] = {
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
        };

        EGLint numConfigs = 0;
        if (!eglChooseConfig(display, cfgAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
            return false;
        }

        EGLint visualId = 0;
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &visualId);
        ANativeWindow_setBuffersGeometry(window, 0, 0, visualId);

        const EGLint ctxAttribs3[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs3);
        const char* glslVersion = "#version 300 es";
        if (context == EGL_NO_CONTEXT) {
            const EGLint ctxAttribs2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
            context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs2);
            glslVersion = "#version 100";
        }
        if (context == EGL_NO_CONTEXT) return false;

        surface = eglCreateWindowSurface(display, config, window, nullptr);
        if (surface == EGL_NO_SURFACE) return false;

        if (!eglMakeCurrent(display, surface, surface, context)) return false;
        eglSwapInterval(display, 1);

        if (!ImGui_ImplOpenGL3_Init(glslVersion)) return false;

        glGenTextures(1, &avatarTex);
        glBindTexture(GL_TEXTURE_2D, avatarTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     menu_ui::kAvatarWidth, menu_ui::kAvatarHeight,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, menu_ui::kAvatarRGBA);
        menu_ui::SetAvatarTexture((ImTextureID)(intptr_t)avatarTex);

        return true;
    }

    void Shutdown() {
        if (avatarTex) {
            glDeleteTextures(1, &avatarTex);
            avatarTex = 0;
        }
        ImGui_ImplOpenGL3_Shutdown();
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
            eglTerminate(display);
        }
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    std::printf("[+] Starting Grape ImGui Overlay Menu...\n");

    auto displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
    int dispW = (displayInfo.width > 0) ? displayInfo.width : 2400;
    int dispH = (displayInfo.height > 0) ? displayInfo.height : 1080;
    int maxSide = std::max(dispW, dispH);
    int minSide = std::min(dispW, dispH);

    std::printf("[+] Display: %dx%d (orientation: %d) -> Landscape UI: %dx%d\n",
                dispW, dispH, displayInfo.orientation, maxSide, minSide);

    ANativeWindow* window = android::ANativeWindowCreator::Create("MenuOverlay", maxSide, maxSide, false);
    if (!window) {
        std::fprintf(stderr, "[-] Failed to create ANativeWindow overlay (root required)\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Compact base font scale for phone screens in landscape
    menu_ui::LoadFonts((float)minSide / 720.0f);

    GlContext gl;
    if (!gl.Init(window)) {
        std::fprintf(stderr, "[-] Failed to initialize EGL / OpenGL ES context\n");
        android::ANativeWindowCreator::Destroy(window);
        ImGui::DestroyContext();
        return 1;
    }

    // Initialize touch in portrait base dimensions (minSide x maxSide) and force
    // landscape orientation when the display is portrait so touch coordinates
    // align with our landscape UI.
    Touch::Init({(float)minSide, (float)maxSide}, false);
    int effectiveOrient = (dispW < dispH) ? 1 : (int)displayInfo.orientation;
    if (effectiveOrient == 0) effectiveOrient = 1;
    Touch::setOrientation(effectiveOrient);

    struct sigaction sa{};
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTERM, SIGINT}) {
        sigaction(sig, &sa, nullptr);
    }

    menu_ui::MenuState state;
    menu_ui::Init(state, (float)maxSide, (float)minSide);

    using clock = std::chrono::steady_clock;
    auto lastTime = clock::now();
    auto lastDisplayPoll = lastTime;

    std::printf("[+] Grape Overlay Menu is active!\n");

    while (g_KeepRunning && !state.exitCompleted) {
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        io.DeltaTime = std::clamp(dt, 0.001f, 0.05f);

        // Poll display rotation every 200ms
        if (now - lastDisplayPoll >= std::chrono::milliseconds(200)) {
            lastDisplayPoll = now;
            displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
            if (displayInfo.width > 0 && displayInfo.height > 0) {
                dispW = displayInfo.width;
                dispH = displayInfo.height;
                maxSide = std::max(dispW, dispH);
                minSide = std::min(dispW, dispH);
            }
            int wantOrient = (dispW < dispH) ? 1 : (int)displayInfo.orientation;
            if (wantOrient == 0) wantOrient = 1;
            Touch::setOrientation(wantOrient);
        }

        if (state.hideRecordChanged) {
            state.hideRecordChanged = false;
            android::ANativeWindowCreator::SetSkipScreenshot(window, state.miscHideRecord);
        }

        if (!state.miscHideRecord) {
            android::ANativeWindowCreator::ProcessMirrorDisplay();
        }

        ImGui_ImplOpenGL3_NewFrame();
        // Match ANativeWindow buffer size (maxSide x maxSide) so OpenGL never stretches vertices
        io.DisplaySize = ImVec2((float)maxSide, (float)maxSide);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

        ImGui::NewFrame();
        menu_ui::Render(state, io.DeltaTime);
        ImGui::Render();
        chams_cfg::Publish(state.chamsEnabled, state.chamsDump, state.chamsColor, state.chamsStrength);

        ImDrawData* drawData = ImGui::GetDrawData();
        const ImVec4& tr = state.activeTouchRect;
        bool shouldBlock = (tr.z > 1.0f && tr.w > 1.0f) && state.miscTouchGrab;

        if (dispW < dispH) {
            // Rotate landscape UI onto portrait screen so it appears horizontal
            RotateDrawDataToLandscapeOnPortrait(drawData, (float)dispW);
            Touch::SetBlockRegion((float)dispW - (tr.y + tr.w), tr.x, tr.w, tr.z, shouldBlock);
        } else {
            Touch::SetBlockRegion(tr.x, tr.y, tr.z, tr.w, shouldBlock);
        }

        glViewport(0, 0, maxSide, maxSide);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
        eglSwapBuffers(gl.display, gl.surface);
    }

    std::printf("[+] Shutting down Grape Overlay cleanly...\n");
    Touch::EmergencyRelease();
    Touch::Close();
    gl.Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    ImGui::DestroyContext();
    return 0;
}

#else
// Non-Android host stub so syntax check / host compilation succeeds cleanly
int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    menu_ui::MenuState state;
    menu_ui::Init(state, 1280.0f, 720.0f);
    ImGui::DestroyContext();
    return 0;
}
#endif
