cmake_minimum_required(VERSION 3.9)
project(example VERSION 1.0.0 LANGUAGES CXX C)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(external/external.cmake)

add_executable(app src/app.cpp)
target_link_libraries(app irkit)
target_compile_features(app PRIVATE cxx_std_17)

add_executable(simple_benchmark src/simple_benchmark.cpp)
target_link_libraries(simple_benchmark irkit rax GSL)
target_compile_features(simple_benchmark PRIVATE cxx_std_17)

add_executable(scoring src/scoring.cpp)
target_link_libraries(scoring irkit rax GSL)
target_compile_features(scoring PRIVATE cxx_std_17)
