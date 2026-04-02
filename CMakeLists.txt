cmake_minimum_required(VERSION 3.10)
project(BloombergOMS CXX)

# ── Compiler settings (C++11, BDE-aligned) ──────────────────────────────────
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings that Bloomberg-style code should pass cleanly
add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# Release build for benchmarks; Debug for tests
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -DDEBUG")

# ── Include paths ────────────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Core OMS library ─────────────────────────────────────────────────────────
set(OMS_SOURCES
    src/FixMessage.cpp
    src/FixParser.cpp
    src/Order.cpp
    src/OrderBook.cpp
    src/PositionManager.cpp
    src/RiskEngine.cpp
    src/OrderBus.cpp
    src/MockExchange.cpp
    src/OmsEngine.cpp
)

add_library(oms_core STATIC ${OMS_SOURCES})
target_link_libraries(oms_core pthread)

# ── Main demo executable ──────────────────────────────────────────────────────
add_executable(oms_demo src/main.cpp)
target_link_libraries(oms_demo oms_core pthread)

# ── Latency benchmark executable ─────────────────────────────────────────────
add_executable(oms_bench bench/LatencyBench.cpp)
target_link_libraries(oms_bench oms_core pthread)

# Benchmark always built as Release for accurate numbers
set_target_properties(oms_bench PROPERTIES
    COMPILE_FLAGS "-O2"
)

# ── Unit tests executable ─────────────────────────────────────────────────────
add_executable(oms_tests tests/tests.cpp)
target_link_libraries(oms_tests oms_core pthread)

# ── Install targets (optional) ────────────────────────────────────────────────
install(TARGETS oms_demo oms_bench oms_tests DESTINATION bin)
install(TARGETS oms_core DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/oms)

# ── Summary ───────────────────────────────────────────────────────────────────
message(STATUS "Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard  : ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler      : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  oms_demo   -- end-to-end demo session")
message(STATUS "  oms_bench  -- latency benchmark (100k orders)")
message(STATUS "  oms_tests  -- unit tests")
