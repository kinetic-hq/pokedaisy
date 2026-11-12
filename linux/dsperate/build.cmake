# The DS backend is entirely opt-in: POCKET_DSPERATE (DSperate, Linux) or POCKET_MELONDS (melonDS, Windows and Linux,
# linux/melonds/build.cmake). Both run the same frontend, companion and field read over ds_core.h.
if(POCKET_MELONDS)
  include(melonds/build.cmake)
  set(pd_ds_core_sources melonds/core_melonds.cpp)
  set(pd_ds_core_target pd_melonds_core)
else()
  set(DSPERATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/dsperate")
  if(NOT EXISTS "${DSPERATE_DIR}/src/core/nds.h")
    message(FATAL_ERROR "Initialize third_party/dsperate for POCKET_DSPERATE")
  endif()
  foreach(feature DSPERATE_SDL DSPERATE_HEADLESS DSPERATE_TESTS DSPERATE_CHEEVOS DSPERATE_NET DSPERATE_VULKAN)
    set(${feature} OFF CACHE BOOL "" FORCE)
  endforeach()
  add_subdirectory("${DSPERATE_DIR}" dsperate EXCLUDE_FROM_ALL)
  # The pinned TrimUI GCC package has no lto1. Keep this optional backend usable
  # with the same SDK as the normal Linux build, without patching DSperate's build.
  set_property(TARGET dsperate_core PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
  set(pd_ds_core_sources dsperate/core_dsperate.cpp)
  set(pd_ds_core_target dsperate_core)
endif()
# The Android app's DS field read (pk_nds_field.cpp) reads RAM through callbacks only, so it builds here unchanged; its
# one read of melonDS's GPU lives in pk_nds_field_gpu.cpp, which isn't built here.
# The companion is the handheld DS core's shell (linux/libretro-ds/companion_ui.cpp: PARTY, BAG, DEX, MAP, GUIDE and
# BATTLE as the Android app draws them), its GUIDE tables generated from the app's Kotlin as that core's build does.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(pd_ds_kotlin ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/kotlin/com/pokedaisy/app/companion/data)
set(pd_ds_guide ${CMAKE_CURRENT_BINARY_DIR}/ds-gen/guide_gen.h)
file(GLOB pd_ds_guide_inputs CONFIGURE_DEPENDS ${pd_ds_kotlin}/Guide*.kt ${pd_ds_kotlin}/Gen4*.kt ${pd_ds_kotlin}/Gen5*.kt)
add_custom_command(OUTPUT ${pd_ds_guide}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/ds-gen
  COMMAND ${Python3_EXECUTABLE} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/dsperate/gen_guide.py ${pd_ds_kotlin} ${pd_ds_guide}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/dsperate/gen_guide.py ${pd_ds_guide_inputs}
  VERBATIM)
add_library(pd_ds_backend STATIC dsperate/frontend.cpp dsperate/companion.cpp dsperate/map_art.cpp dsperate/save_edit_ds.cpp ${pd_ds_core_sources}
  dsperate/party_view.cpp dsperate/bag_view.cpp dsperate/dex_view.cpp dsperate/guide_view.cpp dsperate/map_view.cpp
  libretro-ds/companion_ui.cpp libretro-ds/battle_strip.cpp ${pd_ds_guide}
  ../app/src/main/cpp/nds/pk_nds_field.cpp)
target_compile_definitions(pd_ds_backend PRIVATE PD_DS_DESKTOP)
target_include_directories(pd_ds_backend PUBLIC dsperate PRIVATE ../app/src/main/cpp/nds ../3ds/desktop libretro-ds ${CMAKE_CURRENT_BINARY_DIR}/ds-gen)
target_link_libraries(pd_ds_backend PUBLIC ${pd_ds_core_target} pd_linux_common)
# The field tables leave each game's unused trailing fields zero on purpose.
target_compile_options(pd_ds_backend PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(pd_ds_backend PUBLIC stdc++fs)
endif()
target_compile_definitions(pokedaisy-linux PRIVATE POCKET_DS)
target_link_libraries(pokedaisy-linux PRIVATE pd_ds_backend)
if(BUILD_TESTING)
  add_executable(linux-ds-display-test dsperate/test_display.cpp)
  target_include_directories(linux-ds-display-test PRIVATE ${SDL2_INCLUDE_DIR})
  target_compile_options(linux-ds-display-test PRIVATE -UNDEBUG)
  target_link_libraries(linux-ds-display-test PRIVATE ${SDL2_LIBRARY})
  add_test(NAME ds-launcher-display COMMAND linux-ds-display-test)
  add_executable(linux-ds-layout-test dsperate/test_layout.cpp)
  target_compile_options(linux-ds-layout-test PRIVATE -UNDEBUG)
  if(WIN32)
    target_link_options(linux-ds-layout-test PRIVATE -static)
  endif()
  add_test(NAME ds-layout COMMAND linux-ds-layout-test)
  add_executable(linux-ds-pieces-test dsperate/test_pieces.cpp)
  target_compile_options(linux-ds-pieces-test PRIVATE -UNDEBUG)
  if(WIN32)
    target_link_options(linux-ds-pieces-test PRIVATE -static)
  endif()
  add_test(NAME ds-pieces COMMAND linux-ds-pieces-test)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_executable(linux-ds-test dsperate/test.cpp)
  target_include_directories(linux-ds-test PRIVATE dsperate ../app/src/main/cpp/nds)
  target_compile_options(linux-ds-test PRIVATE -UNDEBUG)
  target_link_libraries(linux-ds-test PRIVATE pd_ds_backend)
  # SAVE EDITOR with its HeartGold / SoulSilver part (POCKET_SAVE_EDIT_NDS names a game and a save to try).
  add_executable(linux-ds-save-edit-test test_save_edit.c)
  target_compile_definitions(linux-ds-save-edit-test PRIVATE POCKET_SAVE_EDIT_DS)
  target_compile_options(linux-ds-save-edit-test PRIVATE -UNDEBUG)
  target_link_libraries(linux-ds-save-edit-test PRIVATE pd_ds_backend)
  if(WIN32)
    target_link_options(linux-ds-save-edit-test PRIVATE -static)
  endif()
  add_test(NAME ds-save-edit COMMAND linux-ds-save-edit-test)
  if(WIN32)
    # SDL's main is replaced by the test's own (no SDL2main), as the host's is.
    target_compile_definitions(linux-ds-display-test PRIVATE SDL_MAIN_HANDLED)
    target_link_options(linux-ds-display-test PRIVATE -static)
    target_link_options(linux-ds-test PRIVATE -static)
  endif()
  set(ds_diagnostic "${CMAKE_CURRENT_BINARY_DIR}/ds-diagnostic.nds")
  add_test(NAME ds-diagnostic-rom COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/dsperate/make_diagnostic.py" "${ds_diagnostic}")
  set_tests_properties(ds-diagnostic-rom PROPERTIES FIXTURES_SETUP ds_diagnostic)
  add_test(NAME ds-core-companion COMMAND linux-ds-test "${CMAKE_CURRENT_SOURCE_DIR}/../app/src/test/resources/fixtures/platinum/mainram.bin" "${ds_diagnostic}")
  add_test(NAME ds-frontend COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/dsperate/test_frontend.py" "${ds_diagnostic}" ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:pokedaisy-linux>)
  set_tests_properties(ds-core-companion ds-frontend PROPERTIES FIXTURES_REQUIRED ds_diagnostic TIMEOUT 90)
  add_test(NAME ds-generated-data COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/dsperate/gen_data.py" --check)
endif()
