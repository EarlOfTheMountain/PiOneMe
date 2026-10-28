/* =========================================================================
 * gk_assets.h — asset root resolution
 *
 * Asset paths are relative to the assets/ directory:
 *
 *   assets/
 *     shaders/   *.vert, *.frag
 *     images/    *.png
 *     sounds/    *.wav
 *     models/    *.obj
 *
 * The root is located automatically: $GK_ASSETS, <exe_dir>/assets,
 * <exe_dir>/../assets or ./assets — whichever exists first.
 * ========================================================================= */
#ifndef GK_ASSETS_H
#define GK_ASSETS_H

#include "gk.h"

#ifdef __cplusplus
extern "C" {
#endif

void gk_assets_init(void);                  /* locate the asset root  */
const char* gk_assets_root(void);           /* "" if not found        */
const char* gk_asset_path(const char* rel); /* "<root>/<rel>" (rotating buffer, valid until 4 more calls) */

#ifdef __cplusplus
}
#endif

#endif /* GK_ASSETS_H */
