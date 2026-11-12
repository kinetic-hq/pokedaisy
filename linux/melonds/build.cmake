# melonDS (third_party/melonds, pinned to 1.1, the app's patch applied at configure time) for the SDL host's DS games:
# its core library only, as the Android app builds it (app/src/main/cpp/CMakeLists.txt), without the app's OpenGL ES
# renderer (the host draws melonDS's software frames). The x86-64 and ARM64 JITs, no GDB stub, no Qt frontend, no LTO.
get_filename_component(MELONDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/melonds" ABSOLUTE)
if(NOT EXISTS "${MELONDS_DIR}/src/NDS.h")
  message(FATAL_ERROR "Initialize third_party/melonds for POCKET_MELONDS (git submodule update --init third_party/melonds)")
endif()
set(BUILD_QT_SDL        OFF CACHE BOOL "" FORCE)
set(ENABLE_OGLRENDERER  OFF CACHE BOOL "" FORCE)
set(ENABLE_GDBSTUB      OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO_RELEASE  OFF CACHE BOOL "" FORCE)
set(ENABLE_LTO          OFF CACHE BOOL "" FORCE)
include("${MELONDS_DIR}/../patches/apply_melonds_patch.cmake")
add_subdirectory("${MELONDS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/melonds" EXCLUDE_FROM_ALL)
set_property(TARGET core PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
# melonDS's Platform layer (files, threads, clocks, the save write-back) is the app's, shared.
add_library(pd_melonds_core STATIC ../app/src/main/cpp/nds/pk_nds_platform.cpp melonds/pk_nds_wide_host.cpp)
target_include_directories(pd_melonds_core PUBLIC ../app/src/main/cpp/nds)
target_link_libraries(pd_melonds_core PUBLIC core)
set_target_properties(pd_melonds_core PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# melonDS's headers test these (its own sources get them from its directory's add_definitions).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
  target_compile_definitions(pd_melonds_core PUBLIC ARCHITECTURE_ARM64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
  target_compile_definitions(pd_melonds_core PUBLIC ARCHITECTURE_x86_64=1)
endif()
if(WIN32)
  target_compile_definitions(pd_melonds_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
if(WIN32)
  # wingdi.h's Polygon() would hide melonDS's Polygon in EXPAND's slices.
  set_source_files_properties(melonds/pk_nds_wide_host.cpp PROPERTIES COMPILE_DEFINITIONS NOGDI)
endif()
