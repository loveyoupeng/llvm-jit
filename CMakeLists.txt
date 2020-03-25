cmake_minimum_required(VERSION 3.13.0)

set(CMAKE_C_COMPILER        /usr/bin/clang-9)
set(CMAKE_C_COMPILER_AR     /usr/bin/llvm-ar-9)
set(CMAKE_C_COMPILER_RANLIB /usr/bin/llvm-ranlib-9)
set(CMAKE_CXX_COMPILER        /usr/bin/clang++-9)
set(CMAKE_CXX_COMPILER_AR     /usr/bin/llvm-ar-9)
set(CMAKE_CXX_COMPILER_RANLIB /usr/bin/llvm-ranlib-9)
set(CMAKE_CXX_STANDARD 17)


project(llvm-jit)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs
  Analysis
  Core
  ExecutionEngine
  InstCombine
  Object
  OrcJIT
  RuntimeDyld
  ScalarOpts
  Support
  nativecodegen
  native)
message(STATUS "Link llvm libraries of ${llvm_libs}")
include_directories(src/main/cpp/include)
add_executable(toy src/main/cpp/toy.cpp)
target_link_libraries(toy ${llvm_libs})


find_package(Threads)
enable_testing()

set(TEST_LIBS
    /usr/local/lib/libgtest.a
    /usr/local/lib/libgtest_main.a
    /usr/local/lib/libgmock.a
    /usr/local/lib/libgmock_main.a)
set(BENCHMARK_LIBS
    /usr/local/lib/libbenchmark.a
    /usr/local/lib/libbenchmark_main.a)
message(STATUS "test lib ${TEST_LIBS}")
message(STATUS "benchmark lib ${BENCHMARK_LIBS}")

add_executable(jit_benchmark src/benchmark/cpp/jit_benchmark.cpp)
target_link_libraries(jit_benchmark ${BENCHMARK_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${llvm_libs})

add_executable(jit_test src/test/cpp/jit_test.cpp)
target_link_libraries(jit_test ${TEST_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${llvm_libs})

add_executable(lljit_test src/test/cpp/lljit_test.cpp)
target_link_libraries(lljit_test ${TEST_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${llvm_libs})

add_executable(thread_local_test src/test/cpp/thread_local_test.cpp)
target_link_libraries(thread_local_test ${TEST_LIBS} ${CMAKE_THREAD_LIBS_INIT})
