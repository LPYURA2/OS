#include "mw_engine.h"
#include <cstdio>
int main(int argc, char **argv) {
    const char *output = argc > 1 ? argv[1] : "frame_cpp.ppm";
    MW_Engine *e = mw_create(480, 270);
    if (!e) return 2;
    mw_scene(e, 47);
    if (!mw_save_ppm(e, output)) { mw_destroy(e); return 3; }
    std::printf("C++ host: saved %s checksum=%08x\n", output, mw_checksum(e));
    mw_destroy(e);
    return 0;
}
