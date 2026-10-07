#pragma once

#include <stddef.h>
#include <stdint.h>

namespace companion_app {

// True when the semicolon-delimited CMD_APP_START name carries [capability]
// inside a comma-separated `cap=` token, e.g. "frmfrg1" in
// "MeshCoreOpen;cap=frmfrg1,mctxt".
bool hasCapability(const uint8_t* app_name, size_t app_name_len,
                   const char* capability);

// MeshCore Open is the reference client for AEIC today. The comparison is
// deliberately permissive for separators/case in the first APP_START token:
// "MeshCoreOpen", "meshcore_open" and "MeshCore Open" are the same app name.
bool appNameIsMeshCoreOpen(const uint8_t* app_name, size_t app_name_len);

}  // namespace companion_app
