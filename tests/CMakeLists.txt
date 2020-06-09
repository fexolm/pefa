include(GoogleTest)
enable_testing()

include_directories(${CMAKE_SOURCE_DIR})

add_executable(test_not_segfaults test_not_segfaults.cpp)
target_link_libraries(test_not_segfaults googletest googletest_main pefa ${DEPS_LIBS})
target_compile_definitions(test_not_segfaults PUBLIC "-DTEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/tests/test_data\"")
gtest_add_tests(TARGET       test_not_segfaults
                TEST_SUFFIX  .noArgs
                TEST_LIST    noArgsTests)

add_executable(test_filter_kernel test_filter_kernel.cpp)
target_link_libraries(test_filter_kernel googletest googletest_main pefa ${DEPS_LIBS})
gtest_add_tests(TARGET       test_filter_kernel
                             TEST_SUFFIX  .noArgs
                             TEST_LIST    noArgsTests)