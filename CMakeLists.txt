cmake_minimum_required(VERSION 3.9)
project(example VERSION 1.0.0 LANGUAGES CXX C)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

option(Query_UNIT "whether or not to build unit tests" ON)
option(Query_INTEGRATION "whether or not to build integration tests" ON)
option(Query_BENCHMARKS "whether or not to build benchmarks" ON)

set(IRKit_BUILD_TEST OFF CACHE INTERNAL "" FORCE)
set(IRKit_BUILD_INTEGRATION_TEST OFF CACHE INTERNAL "" FORCE)
set(IRKit_BUILD_TEST OFF CACHE INTERNAL "" FORCE)
set(IRKit_BUILD_BENCHMARKS OFF CACHE INTERNAL "" FORCE)

include(external/external.cmake)

add_executable(app src/app.cpp)
target_link_libraries(app irkit)
target_compile_features(app PRIVATE cxx_std_17)

add_executable(simple_benchmark src/simple_benchmark.cpp)
target_link_libraries(simple_benchmark irkit rax GSL nlohmann_json ranges type_safe debug_assert)
target_compile_features(simple_benchmark PRIVATE cxx_std_17)

add_executable(scoring src/scoring.cpp)
target_link_libraries(scoring irkit rax GSL nlohmann_json ranges type_safe debug_assert)
target_compile_features(scoring PRIVATE cxx_std_17)

add_executable(taat src/taat.cpp)
target_link_libraries(taat irkit rax GSL nlohmann_json ranges type_safe debug_assert)
target_compile_features(taat PRIVATE cxx_std_17)

if (Query_UNIT OR Query_INTEGRATION)
    enable_testing()
    cmake_policy(SET CMP0054 NEW)
    cmake_policy(SET CMP0057 NEW)
    include(GoogleTest)
    configure_file(scripts/run-tests.sh run-tests.sh COPYONLY)
    if (Query_UNIT)
        add_subdirectory(tests/unit)
    endif()
    if (Query_INTEGRATION)
        add_subdirectory(tests/integration)
    endif()
endif()

if (Query_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
