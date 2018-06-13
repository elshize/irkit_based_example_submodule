cmake_minimum_required(VERSION 3.9)
project(example VERSION 1.0.0 LANGUAGES CXX C)

include(external/external.cmake)

add_executable(app src/app.cpp)
target_link_libraries(app irkit)
target_compile_features(app PRIVATE cxx_std_17)

add_executable(print_postings src/print_postings.cpp)
target_link_libraries(print_postings irkit rax)
target_compile_features(print_postings PRIVATE cxx_std_17)
