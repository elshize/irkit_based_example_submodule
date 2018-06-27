SUITE=$1

if [ "$SUITE" == "unit" ]; then
    cmake -D CMAKE_BUILD_TYPE=Release \
        -D IRKit_BUILD_TEST:BOOL=OFF \
        -D Query_UNIT:BOOL=ON \
        -D Query_INTEGRATION:BOOL=OFF \
        -D Query_BENCHMARKS:BOOL=OFF \
        .
    cmake --build .
    ctest
elif [ "$SUITE" == "integration" ]; then
    cmake -D CMAKE_BUILD_TYPE=Release \
        -D IRKit_BUILD_TEST:BOOL=OFF \
        -D Query_UNIT:BOOL=OFF \
        -D Query_INTEGRATION:BOOL=ON \
        -D Query_BENCHMARKS:BOOL=OFF \
        .
    cmake --build .
    ctest
elif [ "$SUITE" == "benchmarks" ]; then
    cmake -D CMAKE_BUILD_TYPE=Release \
        -D IRKit_BUILD_TEST:BOOL=OFF \
        -D Query_UNIT:BOOL=OFF \
        -D Query_INTEGRATION:BOOL=OFF \
        -D Query_BENCHMARKS:BOOL=ON \
        .
    cmake --build .
    ./benchmarks/all.sh
fi
