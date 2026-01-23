cmake_minimum_required(VERSION 3.16)
project(Luwow VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib_archive)

# Add targets for this project's modules
add_library(Luwow.Engine STATIC)
add_executable(Luwow.LCompile)
add_executable(Luwow.LRun)
add_executable(Luwow.LDebug)

if(WIN32)
    # Define WIN32_LEAN_AND_MEAN to exclude rarely-used APIs.
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
    )
endif()

# Set up target includes
target_include_directories(Luwow.Engine PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Engine
)

target_include_directories(Luwow.LRun PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LRun
    ${CMAKE_SOURCE_DIR}/src/Engine
)

target_include_directories(Luwow.LCompile
    PRIVATE ${CMAKE_SOURCE_DIR}/src/LCompile
    ${CMAKE_SOURCE_DIR}/src/Engine
)

target_include_directories(Luwow.LDebug PRIVATE
    ${CMAKE_SOURCE_DIR}/src/LDebug
    ${CMAKE_SOURCE_DIR}/src/Engine
)

target_link_libraries(Luwow.Engine PUBLIC
    Luau.VM Luau.Common
)

target_link_libraries(Luwow.LRun PRIVATE
    Luwow.Engine
)

target_link_libraries(Luwow.LCompile PRIVATE
    Luwow.Engine
    Luau.Ast
    Luau.Compiler
    Luau.VM
    Luau.Common
)

target_link_libraries(Luwow.LDebug PRIVATE
    Luwow.Engine
    Luau.Ast
    Luau.Compiler
    Luau.VM
    Luau.Common
)

# Set up target properties
set_target_properties(Luwow.LCompile PROPERTIES OUTPUT_NAME lcompile)
set_target_properties(Luwow.LRun PROPERTIES OUTPUT_NAME lrun)
set_target_properties(Luwow.LDebug PROPERTIES OUTPUT_NAME ldebug)

# Add sources for this project's modules
include(Sources.cmake)

# Don't build stuff we don't need
option(LUAU_BUILD_CLI "Build CLI" OFF)
option(LUAU_BUILD_TESTS "Build tests" OFF)

# Add Luau subdirectory
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/luau)

# Add Google's CPPDAP
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/cppdap)

# Set up paths for sssooonnnggg/luau-debugger dependencies
# Luau path
if(NOT DEFINED LUAU_ROOT)
  set(LUAU_ROOT ${CMAKE_SOURCE_DIR}/extern/luau)
endif()

# CPPDAP path
if(NOT DEFINED CPP_DAP_ROOT)
  set(CPP_DAP_ROOT ${CMAKE_SOURCE_DIR}/extern/cppdap)
endif()

# Add Luau.Debugger
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/luau-debugger/debugger)

# Add LRemoteDebug subdirectory
add_subdirectory(${CMAKE_SOURCE_DIR}/src/LRemoteDebug)
