cmake_minimum_required(VERSION 3.24)
project(tinyvmm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(TINYVMM_BUILD_TESTS "Build unit tests" ON)
option(TINYVMM_WERROR "Treat warnings as errors" OFF)

set(TINYVMM_X86_64 OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(TINYVMM_X86_64 ON)
endif()

add_library(tinyvmm_warnings INTERFACE)
target_compile_options(tinyvmm_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
if(TINYVMM_WERROR)
  target_compile_options(tinyvmm_warnings INTERFACE -Werror)
endif()

add_library(tinyvmm_core STATIC
  src/boot/cmdline.cc
  src/boot/kernel_image.cc
  src/boot/linux_loader.cc
  src/common/file.cc
  src/devices/uart8250.cc
  src/memory/guest_memory.cc
  src/trace/boot_trace.cc
  src/trace/marker_scanner.cc
  src/x86/e820.cc
  src/x86/gdt.cc
  src/x86/page_tables.cc
  src/x86/zero_page.cc
)
target_include_directories(tinyvmm_core PUBLIC src)
target_link_libraries(tinyvmm_core PRIVATE tinyvmm_warnings)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_library(tinyvmm_kvm STATIC
    src/kvm/kvm.cc
    src/vmm/smoke.cc
  )
  if(TINYVMM_X86_64)
    target_sources(tinyvmm_kvm PRIVATE src/kvm/x86_cpu.cc)
  endif()
  target_link_libraries(tinyvmm_kvm PUBLIC tinyvmm_core PRIVATE tinyvmm_warnings)

endif()

if(TINYVMM_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(tinyvmm_tests
    tests/boot_trace_test.cc
    tests/cmdline_test.cc
    tests/e820_test.cc
    tests/gdt_test.cc
    tests/guest_memory_test.cc
    tests/kernel_image_test.cc
    tests/linux_loader_test.cc
    tests/marker_scanner_test.cc
    tests/page_tables_test.cc
    tests/smoke_guests_test.cc
    tests/uart8250_test.cc
    tests/zero_page_test.cc
  )
  target_link_libraries(tinyvmm_tests PRIVATE tinyvmm_core tinyvmm_warnings GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(tinyvmm_tests)
endif()
