find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(test_taat test_taat.cpp)
target_link_libraries(test_taat irkit gtest gmock rax GSL Threads::Threads)
target_compile_features(test_taat PRIVATE cxx_std_17)
gtest_add_tests(TARGET test_taat TEST_PREFIX "unit.")
