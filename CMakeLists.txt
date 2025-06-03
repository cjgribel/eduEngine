cmake_minimum_required(VERSION 3.11)

#
# Project
#
project(DA376B_Labs
  VERSION 1.0
  LANGUAGES CXX
)

message(STATUS "Starting CMake configuration for DA376B_Labs project")

#
# CPP
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

#
# Compiler
#
if(APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION=1)
    message(STATUS "Apple platform detected, setting GL_SILENCE_DEPRECATION=1")
else()
    add_compile_definitions(GL_SILENCE_DEPRECATION=0)
    message(STATUS "Non-Apple platform detected")
endif()

# 'target_include_directories' if target specific
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

#
# OpenGL
#
find_package(OpenGL REQUIRED)
message(STATUS "Found OpenGL")
message(STATUS "OpenGL include dir: ${OPENGL_INCLUDE_DIR}")
message(STATUS "OpenGL libraries: ${OPENGL_LIBRARIES}")

#
# Lua
#
# find_package(Lua REQUIRED)
# message(STATUS "Lua found")
# message(STATUS "Found Lua: ${LUA_VERSION_STRING}")
# message(STATUS "Lua include dir: ${LUA_INCLUDE_DIR}")
# message(STATUS "Lua library: ${LUA_LIBRARY}")

#
# Git
#
find_package (Git REQUIRED)
message(STATUS "Found Git: ${GIT_EXECUTABLE}")

include(FetchContent)

#
# Fetch SDL
#
message(STATUS "Fetching SDL2")
FetchContent_Declare(
    sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.30.2 # April 2nd 2024
)
FetchContent_GetProperties(sdl2)
if(NOT sdl2_POPULATED)
    FetchContent_Populate(sdl2)
    add_subdirectory(${sdl2_SOURCE_DIR} ${sdl2_BINARY_DIR})
    message(STATUS "SDL2 populated and added to the build")
endif()
include_directories(${sdl2_SOURCE_DIR}/include)

#
# Fetch glew
# https://github.com/Perlmint/glew-cmake/blob/master/glew-cmake/fetch-content.cmake
#
message(STATUS "Fetching GLEW")
include(FetchContent)
FetchContent_Declare(
  glew
  GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
  GIT_TAG        origin/master
)
FetchContent_GetProperties(glew)
if(NOT glew_POPULATED)
  FetchContent_Populate(glew)
  add_subdirectory(${glew_SOURCE_DIR} ${glew_BINARY_DIR} EXCLUDE_FROM_ALL)
  message(STATUS "GLEW populated and added to the build")
endif()

#
# Find or fetch Zlib
# Required by Assimp. Assimp's packaged version is too old to use.
#
find_package(ZLIB QUIET)
if (NOT ZLIB_FOUND)
    message(STATUS "System Zlib not found, fetching Zlib...")

    FetchContent_Declare(
        zlib
        URL https://zlib.net/zlib-1.3.1.tar.gz
        URL_HASH SHA256=9c5919e0a086e0c59f7c64b9816a30882d89ecf9b7bd774945874bc4a93d7b82
    )
    FetchContent_MakeAvailable(zlib)
endif()

#
# Fetch Assimp
#
message(STATUS "Fetching Assimp")
#set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "Build zlib" FORCE)
set(ASSIMP_BUILD_ZLIB OFF CACHE BOOL "Don't build internal zlib" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build Assimp tools" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build Assimp tests" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Install Assimp" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "Inject debug postfix" FORCE)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assimp/CMakeLists.txt")
    add_subdirectory("assimp")
    message(STATUS "Assimp added from local source")
else()
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.1
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(assimp)
    message(STATUS "Assimp fetched and made available")
endif()
include_directories(${assimp_SOURCE_DIR})
# Set C++14 standard for Assimp 5.0.1
#set_target_properties(assimp PROPERTIES
#    CXX_STANDARD 14
#    CXX_STANDARD_REQUIRED ON
#    CXX_EXTENSIONS OFF
#)

#
# Fetch STB
#
message(STATUS "Fetching STB")
FetchContent_Declare(
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_MakeAvailable(stb)
include_directories(${stb_SOURCE_DIR})
message(STATUS "STB fetched")

#
# Fetch GLM 
# https://github.com/g-truc/glm
#
message(STATUS "Fetching GLM")
# set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
# set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	bf71a834948186f4097caa076cd2663c69a10e1e #refs/tags/1.0.1
)
FetchContent_MakeAvailable(glm)
message(STATUS "GLM fetched and made available")

#
# Fetch enTT (header-only)
#
message(STATUS "Fetching enTT")
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG        v3.13.2
)
FetchContent_MakeAvailable(entt)
include_directories(${entt_SOURCE_DIR}/src) 
message(STATUS "enTT fetched and included")

#
# Fetch Dear ImGui
#
message(STATUS "Fetching Dear ImGui")
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.89.9)
FetchContent_MakeAvailable(imgui)
include_directories(${imgui_SOURCE_DIR})
include_directories(${imgui_SOURCE_DIR}/backends)
message(STATUS "Dear ImGui fetched and included")

#
# Targets
#

# Module1
message(STATUS "Creating executable target for Module1")
add_executable(Module1 
    Module1/main.cpp
    Module1/Game.cpp 
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GLDebugMessageCallback.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glmcommon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InputManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RenderableMesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ForwardRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ShapeRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Log.cpp
    )

set_target_properties(Module1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Module1"
)

target_link_libraries(Module1 PRIVATE 
    SDL2 
    assimp 
    ZLIB::ZLIB 
    libglew_static 
    glm::glm 
    ${OPENGL_LIBRARIES})

# Post-build commands
add_custom_command(TARGET Module1 POST_BUILD
    # Copy SDL2 DLL to the build directory (for Windows)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2>
        $<TARGET_FILE_DIR:Module1>
    # Copy assimp DLL to the build directory (for Windows)
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<TARGET_FILE_DIR:assimp>"
        $<TARGET_FILE_DIR:Module1>
)
message(STATUS "Post-build commands to copy SDL2 and Assimp DLLs to Module1 folder")

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(TARGET Module1 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    message(STATUS "Set Visual Studio debugger working directory")
endif()

# Module2 ...

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Module1)
    message(STATUS "Set Visual Studio startup project to Module1")
endif()

# For 'make install'
#install(TARGETS Module1 Module2)
# message(STATUS "Install targets

# Disable testing for now
# enable_testing()
# add_subdirectory(tests)
#