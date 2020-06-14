cmake_minimum_required(VERSION 3.16)
project(pefa)

set(CMAKE_CXX_STANDARD 17)

include(cmake/options.cmake)
if (ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif ()

set(PEFA_DEPS)

list(APPEND CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX})

find_package(LLVM REQUIRED)
find_package(Arrow REQUIRED arrow_shared)

set(PEFA_DEPS LLVM arrow_shared)

include_directories(vendor)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LLVM_INCLUDE_DIR})
include_directories(${Arrow_INCLUDE_DIR})

add_library(pefa STATIC
        pefa/backends/naive/backend.cpp
        pefa/kernels/filter.cpp
        pefa/api/expressions.cpp
        pefa/api/exceptions.cpp
        pefa/jit/jit.cpp
        )

target_link_libraries(pefa ${PEFA_DEPS})

if(ENABLE_TESTS)
    include(${CMAKE_ROOT}/Modules/FindGTest.cmake)
    find_package(GTest REQUIRED)
    set(PEFA_TEST_DEPS ${GTEST_BOTH_LIBRARIES} pthread)
    include_directories(${GTest_INCLUDE_DIR})
    add_subdirectory(tests)
endif()

if(ENABLE_BENCHMARKS)
    find_package(benchmark REQUIRED)
endif()