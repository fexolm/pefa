include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

set(PEFA_DEPS_PREFIX ${CMAKE_BINARY_DIR}/deps)
set(PEFA_DEPS_SOURCES_PREFIX ${PEFA_DEPS_PREFIX}/sources)
set(PEFA_DEPS_BINARIES_PREFIX ${PEFA_DEPS_PREFIX}/build)
set(PEFA_DEPS_INSTALL_PREFIX ${PEFA_DEPS_PREFIX}/install)

set(DEPS_LIBRARY_DIR ${PEFA_DEPS_INSTALL_PREFIX}/lib)
set(DEPS_INCLUDE_DIR ${PEFA_DEPS_INSTALL_PREFIX}/include)

set(DEPS_LIST "")
set(DEPS_LIBS "")

ExternalProject_Add(arrow-build
        GIT_REPOSITORY    https://github.com/apache/arrow.git
        GIT_TAG           apache-arrow-0.17.1
        GIT_SHALLOW       true
        GIT_PROGRESS      true
        SOURCE_DIR        "${PEFA_DEPS_SOURCES_PREFIX}/arrow"
        BINARY_DIR        "${PEFA_DEPS_BINARIES_PREFIX}/arrow"
        INSTALL_DIR       "${PEFA_DEPS_INSTALL_PREFIX}"
        CMAKE_ARGS
                          -DARROW_CSV=ON
        # TODO: for some reason arrow downloads jemalloc itself and use it, but it doesn't install it
                          -DARROW_JEMALLOC=OFF
                          -DCMAKE_INSTALL_PREFIX=${PEFA_DEPS_INSTALL_PREFIX}
                          -DCMAKE_CXX_FLAGS="-I${DEPS_INCLUDE_DIR} -L${DEPS_LIBRARY_DIR}"
                          -DARROW_USE_ASAN=${ENABLE_ASAN}
        SOURCE_SUBDIR     "cpp"
)

list(APPEND DEPS_LIST arrow-build)
add_library(arrow STATIC IMPORTED)
set_target_properties(arrow PROPERTIES IMPORTED_LOCATION ${DEPS_LIBRARY_DIR}/libarrow.a)
list(APPEND DEPS_LIBS pthread arrow)

if(ENABLE_TESTS)
ExternalProject_Add(googletest-build
        GIT_TAG           release-1.10.0
        GIT_REPOSITORY    https://github.com/google/googletest.git
        GIT_SHALLOW       true
        GIT_PROGRESS      true

        SOURCE_DIR        "${PEFA_DEPS_SOURCES_PREFIX}/googletest"
        BINARY_DIR        "${PEFA_DEPS_BINARIES_PREFIX}/googletest"
        INSTALL_DIR       "${PEFA_DEPS_INSTALL_PREFIX}"

        CMAKE_ARGS
                          -DCMAKE_INSTALL_PREFIX=${PEFA_DEPS_INSTALL_PREFIX}
)

list(APPEND DEPS_LIST googletest-build)
add_library(googletest STATIC IMPORTED)
set_target_properties(googletest PROPERTIES IMPORTED_LOCATION ${DEPS_LIBRARY_DIR}/libgtest.a)
add_library(googletest_main STATIC IMPORTED)
set_target_properties(googletest_main PROPERTIES IMPORTED_LOCATION ${DEPS_LIBRARY_DIR}/libgtest_main.a)

set(ARROW_TESTING_PREFIX ${PEFA_DEPS_SOURCES_PREFIX}/arrow/cpp/src/arrow/testing/)
set(ARROW_TESTING_SOURCES
        ${ARROW_TESTING_PREFIX}/generator.cc
        ${ARROW_TESTING_PREFIX}/gtest_util.cc
        ${ARROW_TESTING_PREFIX}/random.cc
        ${ARROW_TESTING_PREFIX}/util.cc
        ${ARROW_TESTING_PREFIX}/../ipc/json_simple.cc)

set_source_files_properties(${ARROW_TESTING_SOURCES} PROPERTIES GENERATED TRUE)

add_library(arrow_testing ${ARROW_TESTING_SOURCES})
target_link_libraries(arrow_testing arrow googletest googletest_main)
target_include_directories(arrow_testing PUBLIC ${ARROW_TESTING_PREFIX}/../..)
add_dependencies(arrow_testing arrow-build googletest-build)
list(APPEND DEPS_LIST arrow_testing)
endif()
