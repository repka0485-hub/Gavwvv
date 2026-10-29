#pragma once
// Shared by menu (root overlay process) and chams.so (inside the game). Keep both copies identical.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef CHAMS_CFG_PATH
#define CHAMS_CFG_PATH "/data/local/tmp/chams.cfg"
#endif

namespace chams_cfg {

constexpr uint32_t kMagic = 0x31534D43;   // 'CMS1', bump on layout change

struct Data {
    uint32_t magic;
    uint32_t on;
    uint32_t dump;
    float    rgb[3];                      // final blend constant, already tinted
};

// Red, Green, Cyan, Yellow, Magenta, White
inline const float (*Presets())[3] {
    static const float p[6][3] = { {1,.15f,.15f}, {.2f,1,.3f}, {.1f,.9f,1}, {1,.9f,.1f}, {1,.2f,.9f}, {1,1,1} };
    return p;
}

// strength 0 = no tint (const 1,1,1), 1 = full preset
inline Data Make(bool on, bool dump, int preset, float strength) {
    if (preset < 0 || preset > 5) preset = 0;
    if (strength < 0.f) strength = 0.f; else if (strength > 1.f) strength = 1.f;
    Data d{}; d.magic = kMagic; d.on = on; d.dump = dump;
    for (int i = 0; i < 3; i++) d.rgb[i] = 1.f + (Presets()[preset][i] - 1.f) * strength;
    return d;
}

// writer side: tmp + rename so the reader never sees a half-written file
inline bool Write(const Data& d) {
    const char* tmp = CHAMS_CFG_PATH ".tmp";
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;
    bool ok = write(fd, &d, sizeof(d)) == (ssize_t)sizeof(d);
    close(fd);
    if (!ok) return false;
    chmod(tmp, 0666);
    return rename(tmp, CHAMS_CFG_PATH) == 0;
}

// writes only on change (call every frame)
inline void Publish(bool on, bool dump, int preset, float strength) {
    static Data last{}; static bool first = true;
    Data d = Make(on, dump, preset, strength);
    if (first || memcmp(&d, &last, sizeof(d)) != 0) { Write(d); last = d; first = false; }
}

// reader side
inline bool Read(Data& d) {
    int fd = open(CHAMS_CFG_PATH, O_RDONLY);
    if (fd < 0) return false;
    Data t{};
    bool ok = read(fd, &t, sizeof(t)) == (ssize_t)sizeof(t) && t.magic == kMagic;
    close(fd);
    if (ok) d = t;
    return ok;
}

} // namespace chams_cfg
