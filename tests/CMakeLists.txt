include(GoogleTest)
enable_testing()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${GTest_INCLUDE_FIR})

file(GLOB_RECURSE ARROW_TESTING_SOURCES "${CMAKE_SOURCE_DIR}/vendor/arrow/*.cc")

add_library(arrow_testing STATIC ${ARROW_TESTING_SOURCES})
target_link_libraries(arrow_testing ${PEFA_DEPS} ${PEFA_TEST_DEPS})

add_executable(test_not_segfaults test_not_segfaults.cpp)
target_link_libraries(test_not_segfaults ${PEFA_DEPS} ${PEFA_TEST_DEPS} arrow_testing pefa)
target_compile_definitions(test_not_segfaults PUBLIC "-DTEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/tests/test_data\"")
add_test(test_not_segfaults test_not_segfaults)

add_executable(test_filter_kernel kernel_tests/test_filter_kernel.cpp)
target_link_libraries(test_filter_kernel ${PEFA_DEPS} ${PEFA_TEST_DEPS} pefa arrow_testing)
add_test(test_filter_kernel test_filter_kernel)

add_executable(test_filter_executor execution_tests/test_filter.cpp)
target_link_libraries(test_filter_executor ${PEFA_DEPS} ${PEFA_TEST_DEPS} pefa arrow_testing)
add_test(test_filter_executor test_filter_executor)

add_custom_target(test COMMAND ${CMAKE_CTEST_COMMAND}
        DEPENDS test_filter_kernel test_not_segfaults)