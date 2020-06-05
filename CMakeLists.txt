cmake_minimum_required(VERSION 3.16)
project(pefa)

set(CMAKE_CXX_STANDARD 20)

include(cmake/options.cmake)
include(cmake/install-deps.cmake)

include_directories(vendor)
include_directories(${DEPS_INCLUDE_DIR})

add_library(pefa
        pefa/backends/naive/backend.cpp
        pefa/expressions.cpp
)
add_dependencies(pefa ${DEPS_LIST})

target_link_libraries(pefa ${DEPS_LIBS})

add_subdirectory(tests)