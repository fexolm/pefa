enable_testing()

add_executable(run-benchmarks benchmarks.cpp)
target_link_libraries(run-benchmarks benchmark::benchmark pefa ${PEFA_DEPS} arrow_testing)
