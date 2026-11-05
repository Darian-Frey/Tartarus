# Tartarus build glue. Included from Descent3/CMakeLists.txt (one marked hook)
# just before add_executable(Descent3), so it can extend the upstream source
# lists. Everything Tartarus adds to the build is declared here.

set(TARTARUS_VERSION "0.1.0-dev")
set(TARTARUS_DIR ${CMAKE_CURRENT_LIST_DIR})

option(TARTARUS_DEFAULT_CLASSIC "Start in Classic Mode on first run (no tartarus.cfg yet)" ON)
if(TARTARUS_DEFAULT_CLASSIC)
  add_compile_definitions(TARTARUS_DEFAULT_CLASSIC=1)
else()
  add_compile_definitions(TARTARUS_DEFAULT_CLASSIC=0)
endif()

list(APPEND HEADERS
  tartarus/binds.h
  tartarus/binds_core.h
  tartarus/cheat_table.h
  tartarus/cheats.h
  tartarus/classic.h
  tartarus/cmdparse.h
  tartarus/console.h
  tartarus/console_core.h
  tartarus/cvar.h
  tartarus/datadir.h
  tartarus/deck.h
  tartarus/deck_controls.h
  tartarus/deck_core.h
  tartarus/deck_save.h
  tartarus/deck_screens.h
  tartarus/datadir_steam.h
  tartarus/display.h
  tartarus/display_prefs.h
  tartarus/graphics.h
  tartarus/mouse_compat.h
  tartarus/mouse_flight.h
  tartarus/level_assist.h
  tartarus/map_scan.h
  tartarus/mouse_flight_core.h
  tartarus/orient_core.h
  tartarus/orient_hud.h
  tartarus/options_tartarus.h
  tartarus/raw_mouse.h
  tartarus/scan_core.h
  tartarus/small_view_layout.h
  tartarus/small_views.h
  tartarus/tartarus.h
  tartarus/version.h
)
list(APPEND CPPS
  tartarus/binds.cpp
  tartarus/binds_core.cpp
  tartarus/cheat_table.cpp
  tartarus/cheats.cpp
  tartarus/classic.cpp
  tartarus/cmdparse.cpp
  tartarus/console.cpp
  tartarus/console_core.cpp
  tartarus/cvar.cpp
  tartarus/datadir.cpp
  tartarus/deck.cpp
  tartarus/deck_controls.cpp
  tartarus/deck_core.cpp
  tartarus/deck_save.cpp
  tartarus/deck_screens.cpp
  tartarus/datadir_steam.cpp
  tartarus/display.cpp
  tartarus/display_prefs.cpp
  tartarus/graphics.cpp
  tartarus/mouse_flight.cpp
  tartarus/level_assist.cpp
  tartarus/map_scan.cpp
  tartarus/mouse_flight_core.cpp
  tartarus/orient_core.cpp
  tartarus/orient_hud.cpp
  tartarus/options_tartarus.cpp
  tartarus/raw_mouse.cpp
  tartarus/raw_mouse_core.cpp
  tartarus/scan_core.cpp
  tartarus/small_view_layout.cpp
  tartarus/small_views.cpp
  tartarus/tartarus.cpp
  tartarus/version.cpp
)

# Regenerate lib/tartarus_version.h on every build (as upstream does for
# d3_version.h). configure_file only rewrites it when the content changes.
add_custom_target(tartarus_version ALL
  COMMAND ${CMAKE_COMMAND}
    -D SOURCE_DIR=${PROJECT_SOURCE_DIR}
    -D TARTARUS_DIR=${TARTARUS_DIR}
    -D TARGET_DIR=${PROJECT_BINARY_DIR}
    -D TARTARUS_VERSION=${TARTARUS_VERSION}
    -P ${TARTARUS_DIR}/version.cmake
  BYPRODUCTS ${PROJECT_BINARY_DIR}/lib/tartarus_version.h
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMENT "Generating tartarus_version.h"
)
# The Descent3 target does not exist yet at include time; attach the
# dependency once the directory has been processed.
cmake_language(DEFER CALL add_dependencies Descent3 tartarus_version)

if(BUILD_TESTING)
  add_subdirectory(tartarus/tests)
endif()
