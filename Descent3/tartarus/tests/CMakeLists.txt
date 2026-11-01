set(CMAKE_FOLDER "tests")

add_executable(
  tartarus_datadir_tests
  datadir_steam_tests.cpp
  ../datadir_steam.cpp
)
target_link_libraries(
  tartarus_datadir_tests
  GTest::gtest_main
)
gtest_discover_tests(tartarus_datadir_tests)
