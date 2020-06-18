cmake_minimum_required(VERSION 3.5)
project(pefa)

set(CMAKE_CXX_STANDARD 17)

include(cmake/options.cmake)

if (ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif ()

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
set(PEFA_DEPS_PREFIX ${CMAKE_BINARY_DIR}/deps)
set(PEFA_DEPS_SOURCES_PREFIX ${PEFA_DEPS_PREFIX}/sources)
set(PEFA_DEPS_BINARIES_PREFIX ${PEFA_DEPS_PREFIX}/build)
set(PEFA_DEPS_INSTALL_PREFIX ${PEFA_DEPS_PREFIX}/install)

set(DEPS_LIBRARY_DIR ${PEFA_DEPS_INSTALL_PREFIX}/lib)
set(DEPS_INCLUDE_DIR ${PEFA_DEPS_INSTALL_PREFIX}/include)

list(APPEND CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX})

find_package(LLVM REQUIRED)
find_package(Arrow REQUIRED arrow_shared)

set(PEFA_DEPS LLVM arrow_shared)

include_directories(vendor)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${LLVM_INCLUDE_DIR})
include_directories(${Arrow_INCLUDE_DIR})
include_directories(${DEPS_INCLUDE_DIR})

file(GLOB_RECURSE SOURCES pefa/*.cpp)

add_library(pefa STATIC ${SOURCES})

target_link_libraries(pefa ${PEFA_DEPS})


if (ENABLE_TESTS)
    include(cmake/install-gtest.cmake)
    set(PEFA_TEST_DEPS ${GOOGLE_TEST_LIBS} pthread)
    add_subdirectory(tests)
endif ()

if (ENABLE_BENCHMARKS)
    find_package(benchmark REQUIRED)
    include_directories(${benchmark_INCLUDE_DIR})
    add_subdirectory(benchmarks)
endif ()