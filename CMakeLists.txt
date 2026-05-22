cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(redis VERSION 0.1.0 LANGUAGES CXX)

option(REDIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(REDIS_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(REDIS_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(REDIS_ENABLE_CLANG_TIDY "Enable clang-tidy if available" OFF)

include(cmake/StandardProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

add_library(redis_core INTERFACE)
target_include_directories(redis_core
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(redis_client src/core/client.cpp)

add_executable(redis_server
  src/server/server.cpp
  src/server/ring_buffer.cpp
)
target_link_libraries(redis_client PRIVATE redis_core)
target_link_libraries(redis_server PRIVATE redis_core)

if(WIN32)
  target_link_libraries(redis_client PRIVATE ws2_32)
  target_link_libraries(redis_server PRIVATE ws2_32)
endif()

message(STATUS "Environment scaffold ready. Add your own .cpp/.hpp files under src/.")

include(CTest)
if(BUILD_TESTING)
  enable_testing()
endif()
