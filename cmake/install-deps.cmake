include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

set(PEFA_DEPS_PREFIX ${CMAKE_BINARY_DIR}/deps)
set(PEFA_DEPS_SOURCES_PREFIX ${PEFA_DEPS_PREFIX}/sources)
set(PEFA_DEPS_BINARIES_PREFIX ${PEFA_DEPS_PREFIX}/build)
set(PEFA_DEPS_INSTALL_PREFIX ${PEFA_DEPS_PREFIX}/install)

set(DEPS_LIBRARY_DIR ${PEFA_DEPS_INSTALL_PREFIX}/lib)
set(DEPS_INCLUDE_DIR ${PEFA_DEPS_INSTALL_PREFIX}/include)

set(DEPS_LIST "")
set(DEPS_LIBS "")

ExternalProject_Add(jemalloc-build
        SOURCE_DIR          "${PEFA_DEPS_SOURCES_PREFIX}/jemalloc"
        URL                 https://github.com/jemalloc/jemalloc/releases/download/4.1.1/jemalloc-4.1.1.tar.bz2
        INSTALL_DIR         ${PEFA_DEPS_INSTALL_PREFIX}
        DOWNLOAD_DIR        "${PEFA_DEPS_SOURCES_PREFIX}/jemalloc"
        BUILD_COMMAND       $(MAKE)
        BUILD_IN_SOURCE     1
        INSTALL_COMMAND     $(MAKE) install

        CONFIGURE_COMMAND
                            ${PEFA_DEPS_SOURCES_PREFIX}/jemalloc/configure
                            --prefix=${PEFA_DEPS_INSTALL_PREFIX}
)

list(APPEND DEPS_LIST jemalloc-build)
add_library(libjemalloc STATIC IMPORTED GLOBAL)
add_library(libjemalloc_pic STATIC IMPORTED GLOBAL)
set_target_properties(libjemalloc PROPERTIES  IMPORTED_LOCATION "${DEPS_LIBRARY_DIR}/libjemalloc.a")
set_target_properties(libjemalloc_pic PROPERTIES IMPORTED_LOCATION "${DEPS_LIBRARY_DIR}/libjemalloc_pic.a")
list(APPEND DEPS_LIBS libjemalloc libjemalloc_pic)

ExternalProject_Add(arrow-build
        GIT_REPOSITORY    https://github.com/apache/arrow.git
        GIT_TAG           apache-arrow-0.17.1
        GIT_SHALLOW       true
        GIT_PROGRESS      true
        SOURCE_DIR        "${PEFA_DEPS_SOURCES_PREFIX}/arrow"
        BINARY_DIR        "${PEFA_DEPS_BINARIES_PREFIX}/arrow"
        INSTALL_DIR       "${PEFA_DEPS_INSTALL_PREFIX}"
        CMAKE_ARGS
                          -DARROW_COMPUTE=ON
                          -DARROW_CSV=ON
                          -DARROW_BUILD_TESTS=OFF
        # TODO: for some reason arrow downloads jemalloc itself and use it
        # it leads to conflict with bundled jemalloc and fails build
        # reenable this, after fix
                          -DARROW_JEMALLOC=OFF
                          -DCMAKE_INSTALL_PREFIX=${PEFA_DEPS_INSTALL_PREFIX}
                          -DCMAKE_CXX_FLAGS="-I${DEPS_INCLUDE_DIR} -L${DEPS_LIBRARY_DIR}"

        SOURCE_SUBDIR     "cpp"
)

list(APPEND DEPS_LIST arrow-build)
add_library(arrow STATIC IMPORTED)
set_target_properties(arrow PROPERTIES IMPORTED_LOCATION ${DEPS_LIBRARY_DIR}/libarrow.a)
list(APPEND DEPS_LIBS pthread arrow)
add_dependencies(arrow-build jemalloc-build)

if(PEFA_BUILD_TESTS)
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
endif() #PEFA_BUILD_TESTS
