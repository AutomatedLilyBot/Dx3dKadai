# CMakeList.txt : CMake project for NodeWars
cmake_minimum_required (VERSION 3.11)

# ----- vcpkg toolchain integration -----
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# ----- MSVC 调试符号（按生成器区分）-----
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif ()
if (MSVC)
  if (CMAKE_GENERATOR MATCHES "Visual Studio")
    # VS 生成器支持 EditAndContinue
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
            "$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>")
  else ()
    # Ninja/NMake 等不支持 EditAndContinue，用 PDB
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
            "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")
  endif ()
endif()


project("NodeWars")

# ----- 头文件路径（你项目的 include/）-----
include_directories(include)
include_directories(src)
# ----- 依赖（仍用 vcpkg 提供）-----
find_package(SFML COMPONENTS Network Graphics Window CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)

# ===== 手动接入本地 Assimp（IMPORTED 目标） =====
# 目录根
set(PROJ_ROOT "${CMAKE_SOURCE_DIR}")

# 头文件目录
set(ASSIMP_INCLUDE_DIR "${PROJ_ROOT}/external/assimp")

# 库与 DLL（external/assimp 下）
set(ASSIMP_BIN_DIR "${PROJ_ROOT}/external")
set(ASSIMP_RELEASE_LIB "${ASSIMP_BIN_DIR}/assimp-vc143-mt.lib")
set(ASSIMP_RELEASE_DLL "${ASSIMP_BIN_DIR}/assimp-vc143-mt.dll")

# 若将来放入 Debug 版（assimp-vc143-mtd.*）则自动使用；没有则回退到 Release
set(ASSIMP_DEBUG_LIB "${ASSIMP_BIN_DIR}/assimp-vc143-mtd.lib")
set(ASSIMP_DEBUG_DLL "${ASSIMP_BIN_DIR}/assimp-vc143-mtd.dll")
if (NOT EXISTS "${ASSIMP_DEBUG_LIB}")
  set(ASSIMP_DEBUG_LIB "${ASSIMP_RELEASE_LIB}")
endif ()
if (NOT EXISTS "${ASSIMP_DEBUG_DLL}")
  set(ASSIMP_DEBUG_DLL "${ASSIMP_RELEASE_DLL}")
endif ()

# 定义 IMPORTED 共享库目标
add_library(assimp::assimp SHARED IMPORTED GLOBAL)
set_target_properties(assimp::assimp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INCLUDE_DIR}"
        IMPORTED_IMPLIB_DEBUG "${ASSIMP_DEBUG_LIB}"
        IMPORTED_LOCATION_DEBUG "${ASSIMP_DEBUG_DLL}"
        IMPORTED_IMPLIB_RELEASE "${ASSIMP_RELEASE_LIB}"
        IMPORTED_LOCATION_RELEASE "${ASSIMP_RELEASE_DLL}"
        IMPORTED_IMPLIB "${ASSIMP_RELEASE_LIB}"   # 单配置生成器兜底
        IMPORTED_LOCATION "${ASSIMP_RELEASE_DLL}"
)

# ----- 源文件收集 -----
file(GLOB_RECURSE HEADERS "src/*.hpp" "src/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")

# ----- 可执行文件 -----
add_executable(NodeWars
        "NodeWars.cpp" "NodeWars.h"
        ${HEADERS} ${SOURCES}
)

# C++ 标准
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET NodeWars PROPERTY CXX_STANDARD 20)
endif()

# MSVC UTF-8
if(MSVC)
  add_compile_options(/utf-8)
endif()

# ----- 链接依赖 -----
target_link_libraries(NodeWars PRIVATE
        SFML::Network
        SFML::Graphics
        SFML::Window
        Microsoft::DirectXTex
        assimp::assimp           # ← 手动接入的 Assimp 目标
)

# 运行时复制 Assimp 对应配置的 DLL 到可执行文件目录
add_custom_command(TARGET NodeWars POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${ASSIMP_DEBUG_DLL},${ASSIMP_RELEASE_DLL}>"
        "$<TARGET_FILE_DIR:NodeWars>"
)

# ----- 资源复制 -----
add_custom_command(TARGET NodeWars POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shader
        $<TARGET_FILE_DIR:NodeWars>/shader
)
add_custom_command(TARGET NodeWars POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/asset
        $<TARGET_FILE_DIR:NodeWars>/asset
)
