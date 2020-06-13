cmake_minimum_required(VERSION 3.16)
project(pefa)

set(CMAKE_CXX_STANDARD 17)

include(cmake/options.cmake)
if (ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif ()
include(cmake/install-deps.cmake)

include_directories(vendor)
include_directories(${DEPS_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(LLVM REQUIRED)

add_library(pefa
        pefa/backends/naive/backend.cpp
        pefa/kernels/filter.cpp
        pefa/api/expressions.cpp
        pefa/api/exceptions.cpp
        pefa/jit/jit.cpp
        )
add_dependencies(pefa ${DEPS_LIST})

target_link_libraries(pefa ${DEPS_LIBS} LLVM)

add_subdirectory(tests)