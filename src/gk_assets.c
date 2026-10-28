/* gk_assets.c — locate the assets/ directory next to the executable */
#include "gk.h"
#include "gk_assets.h"

#include <unistd.h>
#include <sys/stat.h>

static char gk__root[512];
static bool gk__found = false;
static bool gk__warned = false;

static bool gk__is_dir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void gk_assets_init(void) {
    char exe[512];
    char dir[512];
    char cand[600];
    ssize_t n;
    char* slash;
    const char* env = getenv("GK_ASSETS");

    gk__root[0] = 0;
    gk__found = false;

    if (env && gk__is_dir(env)) {
        snprintf(gk__root, sizeof gk__root, "%s", env);
        gk__found = true;
    }

    if (!gk__found) {
        n = readlink("/proc/self/exe", exe, sizeof exe - 1);
        if (n > 0) {
            exe[n] = 0;
            slash = strrchr(exe, '/');
            if (slash) {
                *slash = 0;
                snprintf(dir, sizeof dir, "%s", exe);

                snprintf(cand, sizeof cand, "%s/assets", dir);
                if (!gk__found && gk__is_dir(cand)) {
                    snprintf(gk__root, sizeof gk__root, "%.500s", cand);
                    gk__found = true;
                }
                snprintf(cand, sizeof cand, "%s/../assets", dir);
                if (!gk__found && gk__is_dir(cand)) {
                    snprintf(gk__root, sizeof gk__root, "%.500s", cand);
                    gk__found = true;
                }
            }
        }
    }

    if (!gk__found && gk__is_dir("assets")) {
        snprintf(gk__root, sizeof gk__root, "assets");
        gk__found = true;
    }

    if (gk__found) {
        GK_LOG("assets: %s\n", gk__root);
    } else {
        GK_WARN("assets directory not found (set $GK_ASSETS)\n");
    }
}

const char* gk_assets_root(void) {
    return gk__root;
}

const char* gk_asset_path(const char* rel) {
    static char bufs[4][600];
    static int  next = 0;
    char* b = bufs[next];
    next = (next + 1) & 3;
    if (!gk__found && !gk__warned) {
        GK_WARN("asset \"%s\" requested but no asset root was found\n", rel);
        gk__warned = true;
    }
    if (!gk__found)
        snprintf(b, 600, "%s", rel);
    else
        snprintf(b, 600, "%.511s/%.80s", gk__root, rel);
    return b;
}
