include(GoogleTest)
enable_testing()

include_directories(${CMAKE_SOURCE_DIR})

add_executable(test_not_segfaults test_not_segfaults.cpp)
target_link_libraries(test_not_segfaults googletest googletest_main pefa ${DEPS_LIBS} arrow_testing)
target_compile_definitions(test_not_segfaults PUBLIC "-DTEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/tests/test_data\"")
add_test(test_not_segfaults test_not_segfaults)

add_executable(test_filter_kernel test_filter_kernel.cpp)
target_link_libraries(test_filter_kernel googletest googletest_main pefa ${DEPS_LIBS} arrow_testing)
add_test(test_filter_kernel test_filter_kernel)

add_custom_target(test COMMAND ${CMAKE_CTEST_COMMAND}
        DEPENDS test_filter_kernel test_not_segfaults)