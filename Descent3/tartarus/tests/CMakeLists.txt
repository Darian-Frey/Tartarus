set(CMAKE_FOLDER "tests")

# Tests cover the pure-C++ parts of Descent3/tartarus/ (no engine headers).

add_executable(
  tartarus_datadir_tests
  datadir_steam_tests.cpp
  ../datadir_steam.cpp
)
target_link_libraries(tartarus_datadir_tests GTest::gtest_main)
gtest_discover_tests(tartarus_datadir_tests)

add_executable(
  tartarus_cvar_tests
  cmdparse_tests.cpp
  cvar_tests.cpp
  ../cmdparse.cpp
  ../cvar.cpp
)
target_link_libraries(tartarus_cvar_tests GTest::gtest_main)
gtest_discover_tests(tartarus_cvar_tests)

add_executable(
  tartarus_console_tests
  console_core_tests.cpp
  ../console_core.cpp
  ../cmdparse.cpp
  ../cvar.cpp
)
target_link_libraries(tartarus_console_tests GTest::gtest_main)
gtest_discover_tests(tartarus_console_tests)

add_executable(
  tartarus_display_tests
  display_prefs_tests.cpp
  ../display_prefs.cpp
)
target_link_libraries(tartarus_display_tests GTest::gtest_main)
gtest_discover_tests(tartarus_display_tests)
