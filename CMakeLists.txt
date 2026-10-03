cmake_minimum_required(VERSION 3.16)
project(thread_safe_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(ENABLE_ASAN "Address + UB sanitizers" OFF)
option(ENABLE_TSAN "Thread sanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
  message(FATAL_ERROR "ASan and TSan cannot be combined")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()
if(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -O1)
  add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)

add_library(cache INTERFACE)
target_include_directories(cache INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cache INTERFACE Threads::Threads)

enable_testing()
add_subdirectory(tests)
add_subdirectory(bench)
