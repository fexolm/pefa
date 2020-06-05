include(GoogleTest)
enable_testing()

include_directories(${CMAKE_SOURCE_DIR})

add_executable(test_projection test_projection.cpp)
target_link_libraries(test_projection googletest googletest_main pefa ${DEPS_LIBS})
target_compile_definitions(test_projection PUBLIC "-DTEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/tests/test_data\"")
gtest_add_tests(TARGET       test_projection
                TEST_SUFFIX  .noArgs
                TEST_LIST    noArgsTests)