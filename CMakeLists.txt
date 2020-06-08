cmake_minimum_required(VERSION 3.16)
project(pefa)

set(CMAKE_CXX_STANDARD 20)

include(cmake/options.cmake)
include(cmake/install-deps.cmake)

if (ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif ()

include_directories(vendor)
include_directories(${DEPS_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM REQUIRED)

add_library(pefa
        pefa/backends/naive/backend.cpp
        pefa/backends/naive/kernels/filter.cpp
        pefa/expressions/expressions.cpp
        pefa/jit/jit.cpp
        )
add_dependencies(pefa ${DEPS_LIST})

target_link_libraries(pefa ${DEPS_LIBS} LLVM)

add_subdirectory(tests)

add_executable(main main.cpp)
target_link_libraries(main LLVM)