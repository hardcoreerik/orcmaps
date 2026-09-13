#pragma once

/* miniz is normally built by CMake, which generates this header from
   miniz_export.h.in based on shared/static library configuration. OrcMaps
   vendors miniz source directly (no CMake build of miniz itself) and only
   ever links it statically into the archive/host-test targets, so the
   export macro is always empty -- this file replaces the generated one.
   See ../../docs/DEPENDENCY_LEDGER.md. */

#ifndef MINIZ_EXPORT
#define MINIZ_EXPORT
#endif
