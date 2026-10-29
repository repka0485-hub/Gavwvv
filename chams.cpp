// chams.cpp: GLES glDrawElements hook, hidden-pass chams
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unordered_set>
#include <android/log.h>
#include "dobby.h"
#include "ChamsCfg.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "crez", __VA_ARGS__)

namespace off {
    // FILL: index counts of player meshes. run dump mode, stand next to a player, read logcat
    static const GLsizei player_idx[] = { 0 /* FILL */ };
}
namespace cfg {
    static float hidden[4] = { 1.f, 0.15f, 0.15f, 1.f };
}

static bool g_on = false, g_dump = false;
static std::unordered_set<GLsizei> g_seen;
static void (*o_draw)(GLenum, GLsizei, GLenum, const void*);
static EGLBoolean (*o_swap)(EGLDisplay, EGLSurface);

static bool is_player(GLsizei c) {
    for (GLsizei p : off::player_idx) if (p && p == c) return true;
    return false;
}

static void h_draw(GLenum mode, GLsizei count, GLenum type, const void* idx) {
    if (g_dump && g_seen.insert(count).second) LOG("idx_count=%d mode=0x%x", count, mode);

    if (g_on && is_player(count)) {
        GLboolean blend = glIsEnabled(GL_BLEND);
        GLboolean dmask;
        GLint df, sr, dr, sa, da;
        GLfloat bc[4];
        glGetIntegerv(GL_DEPTH_FUNC, &df);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &dmask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &sr);   glGetIntegerv(GL_BLEND_DST_RGB, &dr);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sa); glGetIntegerv(GL_BLEND_DST_ALPHA, &da);
        glGetFloatv(GL_BLEND_COLOR, bc);

        // GREATER = only occluded pixels; out = dst * const (tints what's behind the wall), alpha kept
        glDepthFunc(GL_GREATER);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendColor(cfg::hidden[0], cfg::hidden[1], cfg::hidden[2], cfg::hidden[3]);
        glBlendFuncSeparate(GL_ZERO, GL_CONSTANT_COLOR, GL_ZERO, GL_ONE);
        o_draw(mode, count, type, idx);

        glDepthFunc(df);
        glDepthMask(dmask);
        glBlendFuncSeparate(sr, dr, sa, da);
        glBlendColor(bc[0], bc[1], bc[2], bc[3]);
        if (!blend) glDisable(GL_BLEND);
    }
    o_draw(mode, count, type, idx);   // normal pass
}

static EGLBoolean h_swap(EGLDisplay d, EGLSurface s) {
    static int f = 0;
    if (++f % 30 == 0) {               // poll menu config ~2x a second
        chams_cfg::Data d;
        if (chams_cfg::Read(d)) {
            g_on = d.on; g_dump = d.dump;
            for (int i = 0; i < 3; i++) cfg::hidden[i] = d.rgb[i];
        } else g_on = g_dump = false;
    }
    return o_swap(d, s);
}

// DobbyHook signature differs between versions (void* vs dobby_dummy_func_t), cast to whatever it takes
template <class R, class O>
static int hook_impl(int (*fn)(void*, R, O), void* t, void* r, void** o) { return fn(t, (R)r, (O)o); }
static int hook(void* t, void* r, void** o) { return hook_impl(DobbyHook, t, r, o); }

static void* init(void*) {
    void *gles = nullptr, *egl = nullptr;
    while (!(gles = dlopen("libGLESv2.so", RTLD_NOW | RTLD_NOLOAD))) sleep(1);
    while (!(egl  = dlopen("libEGL.so",    RTLD_NOW | RTLD_NOLOAD))) sleep(1);
    void* d = dlsym(gles, "glDrawElements");
    void* s = dlsym(egl,  "eglSwapBuffers");
    if (!d || !s) { LOG("sym fail d=%p s=%p", d, s); return nullptr; }
    hook(d, (void*)h_draw, (void**)&o_draw);
    hook(s, (void*)h_swap, (void**)&o_swap);
    LOG("hooked draw=%p swap=%p", d, s);
    return nullptr;
}

__attribute__((constructor)) static void ctor() {
    pthread_t t; pthread_create(&t, nullptr, init, nullptr);
}
