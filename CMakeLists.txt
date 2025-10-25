# CMakeList.txt : CMake project for golfgame, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)

# vcpkg toolchain integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("golfgame")

# Include directories
include_directories(include)

# Find SFML package
find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)

# Collect header files
file(GLOB_RECURSE HEADERS "include/**/*.hpp" "include/**/*.h")

# Collect source files
file(GLOB_RECURSE SOURCES "src/**/*.cpp" "src/**/*.c")

# Add source to this project's executable.
add_executable (golfgame "golfgame.cpp" "golfgame.h" ${HEADERS} ${SOURCES}
        include/core/gfx/Vertex.hpp)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET golfgame PROPERTY CXX_STANDARD 20)
endif()

# Link SFML libraries (this also includes SFML headers)
target_link_libraries(golfgame PRIVATE
        SFML::Network
        SFML::Graphics
        SFML::Window
        SFML::Audio
        SFML::System
    $<$<BOOL:${SFML_audio_FOUND}>:sfml-audio>
)

# Copy shader directory to output directory
add_custom_command(TARGET golfgame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shader
    $<TARGET_FILE_DIR:golfgame>/shader
)

# TODO: Add tests and install targets if needed.
