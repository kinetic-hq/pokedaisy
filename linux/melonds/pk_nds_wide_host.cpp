// GPL-3.0. The app's EXPAND (app/src/main/cpp/nds/pk_nds_wide.cpp) as the SDL host builds it: on Windows, melonDS's
// headers bring in <windows.h>, whose near / far macros and wingdi.h's Polygon() would break that file, so they're
// read first and the macros dropped (NOGDI is set for this file in build.cmake).
#include <NDS.h>
#include <GPU.h>
#include <GPU3D.h>
#include <GPU3D_Soft.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#include "../../app/src/main/cpp/nds/pk_nds_wide.cpp"
