
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
    add_dependencies(googletest googletest-build)
    add_dependencies(googletest_main googletest-build)
    set(GOOGLE_TEST_LIBS googletest googletest_main)
endif()