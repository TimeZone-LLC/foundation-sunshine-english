# linux specific compile definitions

add_compile_definitions(SUNSHINE_PLATFORM="linux")

# AppImage
if(${SUNSHINE_BUILD_APPIMAGE})
    # use relative assets path for AppImage
    string(REPLACE "${CMAKE_INSTALL_PREFIX}" ".${CMAKE_INSTALL_PREFIX}" SUNSHINE_ASSETS_DIR_DEF ${SUNSHINE_ASSETS_DIR})
endif()

if(${SUNSHINE_BUILD_FLATPAK})
    list(APPEND SUNSHINE_DEFINITIONS SUNSHINE_BUILD_FLATPAK=1)
endif()

# cuda
set(CUDA_FOUND OFF)
if(${SUNSHINE_ENABLE_CUDA})
    include(CheckLanguage)
    check_language(CUDA)

    if(CMAKE_CUDA_COMPILER)
        set(CUDA_FOUND ON)
        enable_language(CUDA)

        message(STATUS "CUDA Compiler Version: ${CMAKE_CUDA_COMPILER_VERSION}")
        set(CMAKE_CUDA_ARCHITECTURES "")

        # https://tech.amikelive.com/node-930/cuda-compatibility-of-nvidia-display-gpu-drivers/
        if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 6.5)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 10)
        elseif(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 6.5)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 50 52)
        endif()

        if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 7.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 11)
        elseif(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER 7.6)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 60 61 62)
        endif()

        # https://docs.nvidia.com/cuda/archive/9.2/cuda-compiler-driver-nvcc/index.html
        if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 9.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 20)
        elseif(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 9.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 70)
        endif()

        # https://docs.nvidia.com/cuda/archive/10.0/cuda-compiler-driver-nvcc/index.html
        if(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 72 75)
        endif()

        # https://docs.nvidia.com/cuda/archive/11.0/cuda-compiler-driver-nvcc/index.html
        if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 11.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 30)
        elseif(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 80)
        endif()

        # https://docs.nvidia.com/cuda/archive/11.8.0/cuda-compiler-driver-nvcc/index.html
        if(CMAKE_CUDA_COMPILER_VERSION VERSION_GREATER_EQUAL 11.8)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 86 87 89 90)
        endif()

        if(CMAKE_CUDA_COMPILER_VERSION VERSION_LESS 12.0)
            list(APPEND CMAKE_CUDA_ARCHITECTURES 35)
        endif()

        # sort the architectures
        list(SORT CMAKE_CUDA_ARCHITECTURES COMPARE NATURAL)

        # message(STATUS "CUDA NVCC Flags: ${CUDA_NVCC_FLAGS}")
        message(STATUS "CUDA Architectures: ${CMAKE_CUDA_ARCHITECTURES}")
    endif()
endif()
if(CUDA_FOUND)
    include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/third-party/nvfbc")
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/cuda.h"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/cuda.cu"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/cuda.cpp"
            "${CMAKE_SOURCE_DIR}/third-party/nvfbc/NvFBC.h")

    add_compile_definitions(SUNSHINE_BUILD_CUDA)
endif()

# libdrm headers describe DMA-BUF formats used by all modern Linux capture paths.
if(${SUNSHINE_ENABLE_DRM} OR ${SUNSHINE_ENABLE_WAYLAND} OR ${SUNSHINE_ENABLE_VULKAN}
        OR ${SUNSHINE_ENABLE_KWIN} OR ${SUNSHINE_ENABLE_PORTAL})
    find_package(LIBDRM REQUIRED)
else()
    set(LIBDRM_FOUND OFF)
endif()
if(LIBDRM_FOUND)
    include_directories(SYSTEM ${LIBDRM_INCLUDE_DIRS})
    if(${SUNSHINE_ENABLE_DRM})
        add_compile_definitions(SUNSHINE_BUILD_DRM)
        list(APPEND PLATFORM_LIBRARIES ${LIBDRM_LIBRARIES})
        list(APPEND PLATFORM_TARGET_FILES
                "${CMAKE_SOURCE_DIR}/src/platform/linux/kmsgrab.cpp")
        list(APPEND SUNSHINE_DEFINITIONS EGL_NO_X11=1)
    endif()
endif()

# The process drops elevated capabilities after constructing narrowly scoped workers.
find_package(LIBCAP REQUIRED)
include_directories(SYSTEM ${LIBCAP_INCLUDE_DIRS})
list(APPEND PLATFORM_LIBRARIES ${LIBCAP_LIBRARIES})

# evdev
include(dependencies/libevdev_Sunshine)

# vaapi
if(${SUNSHINE_ENABLE_VAAPI})
    find_package(Libva)
else()
    set(LIBVA_FOUND OFF)
endif()
if(LIBVA_FOUND)
    add_compile_definitions(SUNSHINE_BUILD_VAAPI)
    include_directories(SYSTEM ${LIBVA_INCLUDE_DIR})
    list(APPEND PLATFORM_LIBRARIES ${LIBVA_LIBRARIES} ${LIBVA_DRM_LIBRARIES})
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/vaapi.h"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/vaapi.cpp")
endif()

# Vulkan video encoding: DMA-BUF import, compute conversion, and FFmpeg Vulkan Video.
if(${SUNSHINE_ENABLE_VULKAN})
    if(SUNSHINE_SYSTEM_VULKAN_HEADERS)
        find_package(VulkanHeaders REQUIRED)
        get_target_property(VULKAN_HEADERS_DIR Vulkan::Headers INTERFACE_INCLUDE_DIRECTORIES)
    else()
        set(VULKAN_HEADERS_DIR "${CMAKE_SOURCE_DIR}/third-party/build-deps/third-party/FFmpeg/Vulkan-Headers/include")
    endif()
    if(NOT EXISTS "${VULKAN_HEADERS_DIR}/vulkan/vulkan.h")
        message(FATAL_ERROR "Vulkan headers not found")
    endif()

    find_library(VULKAN_LIBRARY NAMES vulkan vulkan-1 REQUIRED)
    find_program(GLSLC_EXECUTABLE glslc)
    if(NOT GLSLC_EXECUTABLE)
        find_program(GLSLANG_EXECUTABLE glslangValidator)
    endif()
    if(NOT GLSLC_EXECUTABLE AND NOT GLSLANG_EXECUTABLE)
        message(FATAL_ERROR "Vulkan shader compiler not found (need glslc or glslangValidator)")
    endif()

    list(APPEND SUNSHINE_DEFINITIONS SUNSHINE_BUILD_VULKAN=1)
    include_directories(SYSTEM ${VULKAN_HEADERS_DIR})
    list(APPEND PLATFORM_LIBRARIES ${VULKAN_LIBRARY})
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/vulkan_encode.h"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/vulkan_encode.cpp")

    set(VULKAN_SHADER_DIR "${CMAKE_BINARY_DIR}/generated-src/shaders")
    set(VULKAN_SHADER_SOURCE "${SUNSHINE_SOURCE_ASSETS_DIR}/linux/assets/shaders/vulkan/rgb2yuv.comp")
    set(VULKAN_SHADER_SPV "${VULKAN_SHADER_DIR}/rgb2yuv.spv")
    set(VULKAN_SHADER_DATA "${VULKAN_SHADER_DIR}/rgb2yuv.spv.inc")
    file(MAKE_DIRECTORY "${VULKAN_SHADER_DIR}")

    if(GLSLC_EXECUTABLE)
        add_custom_command(
                OUTPUT "${VULKAN_SHADER_SPV}"
                COMMAND ${GLSLC_EXECUTABLE} -O "${VULKAN_SHADER_SOURCE}" -o "${VULKAN_SHADER_SPV}"
                DEPENDS "${VULKAN_SHADER_SOURCE}"
                COMMENT "Compiling Vulkan RGB-to-YUV shader"
                VERBATIM)
    else()
        add_custom_command(
                OUTPUT "${VULKAN_SHADER_SPV}"
                COMMAND ${GLSLANG_EXECUTABLE} -V -o "${VULKAN_SHADER_SPV}" "${VULKAN_SHADER_SOURCE}"
                DEPENDS "${VULKAN_SHADER_SOURCE}"
                COMMENT "Compiling Vulkan RGB-to-YUV shader"
                VERBATIM)
    endif()
    add_custom_command(
            OUTPUT "${VULKAN_SHADER_DATA}"
            COMMAND ${CMAKE_COMMAND} -DSPV_FILE=${VULKAN_SHADER_SPV} -DOUT_FILE=${VULKAN_SHADER_DATA}
                    -P "${CMAKE_SOURCE_DIR}/cmake/scripts/binary_to_c.cmake"
            DEPENDS "${VULKAN_SHADER_SPV}"
            COMMENT "Embedding Vulkan RGB-to-YUV shader"
            VERBATIM)
    add_custom_target(vulkan_shaders DEPENDS "${VULKAN_SHADER_DATA}")
    list(APPEND SUNSHINE_TARGET_DEPENDENCIES vulkan_shaders)
endif()

# wayland
if(${SUNSHINE_ENABLE_WAYLAND})
    find_package(Wayland)
else()
    set(WAYLAND_FOUND OFF)
endif()
if(WAYLAND_FOUND)
    add_compile_definitions(SUNSHINE_BUILD_WAYLAND)

    if(NOT SUNSHINE_SYSTEM_WAYLAND_PROTOCOLS)
        set(WAYLAND_PROTOCOLS_DIR "${CMAKE_SOURCE_DIR}/third-party/wayland-protocols")
    else()
        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
        pkg_check_modules(WAYLAND_PROTOCOLS wayland-protocols REQUIRED)
    endif()

    GEN_WAYLAND("${WAYLAND_PROTOCOLS_DIR}" "unstable/xdg-output" xdg-output-unstable-v1)
    GEN_WAYLAND("${WAYLAND_PROTOCOLS_DIR}" "unstable/linux-dmabuf" linux-dmabuf-unstable-v1)
    GEN_WAYLAND("${CMAKE_SOURCE_DIR}/third-party/wlr-protocols" "unstable" wlr-screencopy-unstable-v1)

    include_directories(
            SYSTEM
            ${WAYLAND_INCLUDE_DIRS}
            ${CMAKE_BINARY_DIR}/generated-src
    )

    list(APPEND PLATFORM_LIBRARIES ${WAYLAND_LIBRARIES} gbm)
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/wlgrab.cpp"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/wayland.h"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/wayland.cpp")
endif()

# Portal and direct-KWin capture share the PipeWire frame transport.
pkg_check_modules(GIO gio-2.0 gio-unix-2.0 REQUIRED)
include_directories(SYSTEM ${GIO_INCLUDE_DIRS})
list(APPEND PLATFORM_LIBRARIES ${GIO_LIBRARIES})

if(${SUNSHINE_ENABLE_KWIN} OR ${SUNSHINE_ENABLE_PORTAL})
    pkg_check_modules(PIPEWIRE libpipewire-0.3 REQUIRED)
else()
    set(PIPEWIRE_FOUND OFF)
endif()
if(PIPEWIRE_FOUND)
    include_directories(SYSTEM ${PIPEWIRE_INCLUDE_DIRS})
    list(APPEND PLATFORM_LIBRARIES ${PIPEWIRE_LIBRARIES})
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/pipewire.cpp")
endif()

set(PORTAL_FOUND OFF)
if(PIPEWIRE_FOUND AND GIO_FOUND AND ${SUNSHINE_ENABLE_PORTAL})
    set(PORTAL_FOUND ON)
    add_compile_definitions(SUNSHINE_BUILD_PORTAL)
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/portalgrab.cpp")
endif()

set(KWIN_FOUND OFF)
if(PIPEWIRE_FOUND AND WAYLAND_FOUND AND ${SUNSHINE_ENABLE_KWIN})
    set(KWIN_FOUND ON)
    add_compile_definitions(SUNSHINE_BUILD_KWIN)
    GEN_WAYLAND("${CMAKE_SOURCE_DIR}/third-party/plasma-wayland-protocols/src/protocols" "" kde-output-order-v1)
    GEN_WAYLAND("${CMAKE_SOURCE_DIR}/third-party/plasma-wayland-protocols/src/protocols" "" zkde-screencast-unstable-v1)
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/kwingrab.cpp")
elseif(${SUNSHINE_ENABLE_KWIN} AND NOT WAYLAND_FOUND)
    message(FATAL_ERROR "SUNSHINE_ENABLE_KWIN requires SUNSHINE_ENABLE_WAYLAND")
endif()

# x11
if(${SUNSHINE_ENABLE_X11})
    find_package(X11)
else()
    set(X11_FOUND OFF)
endif()
if(X11_FOUND)
    add_compile_definitions(SUNSHINE_BUILD_X11)
    include_directories(SYSTEM ${X11_INCLUDE_DIR})
    list(APPEND PLATFORM_LIBRARIES ${X11_LIBRARIES})
    list(APPEND PLATFORM_TARGET_FILES
            "${CMAKE_SOURCE_DIR}/src/platform/linux/x11grab.h"
            "${CMAKE_SOURCE_DIR}/src/platform/linux/x11grab.cpp")
endif()

if(NOT ${CUDA_FOUND}
        AND NOT ${WAYLAND_FOUND}
        AND NOT ${X11_FOUND}
        AND NOT ${LIBDRM_FOUND}
        AND NOT ${KWIN_FOUND}
        AND NOT ${PORTAL_FOUND}
        AND NOT ${LIBVA_FOUND})
    message(FATAL_ERROR "Couldn't find a supported Linux capture or encode backend")
endif()

# tray icon
if(${SUNSHINE_ENABLE_TRAY})
    pkg_check_modules(APPINDICATOR ayatana-appindicator3-0.1)
    if(APPINDICATOR_FOUND)
        list(APPEND SUNSHINE_DEFINITIONS TRAY_AYATANA_APPINDICATOR=1)
    else()
        pkg_check_modules(APPINDICATOR appindicator3-0.1)
        if(APPINDICATOR_FOUND)
            list(APPEND SUNSHINE_DEFINITIONS TRAY_LEGACY_APPINDICATOR=1)
        endif ()
    endif()
    pkg_check_modules(LIBNOTIFY libnotify)
    if(NOT APPINDICATOR_FOUND OR NOT LIBNOTIFY_FOUND)
        set(SUNSHINE_TRAY 0)
        message(WARNING "Missing appindicator or libnotify, disabling tray icon")
        message(STATUS "APPINDICATOR_FOUND: ${APPINDICATOR_FOUND}")
        message(STATUS "LIBNOTIFY_FOUND: ${LIBNOTIFY_FOUND}")
    else()
        include_directories(SYSTEM ${APPINDICATOR_INCLUDE_DIRS} ${LIBNOTIFY_INCLUDE_DIRS})
        link_directories(${APPINDICATOR_LIBRARY_DIRS} ${LIBNOTIFY_LIBRARY_DIRS})

        list(APPEND PLATFORM_TARGET_FILES "${CMAKE_SOURCE_DIR}/third-party/tray/src/tray_linux.c")
        list(APPEND SUNSHINE_EXTERNAL_LIBRARIES ${APPINDICATOR_LIBRARIES} ${LIBNOTIFY_LIBRARIES})
    endif()
else()
    set(SUNSHINE_TRAY 0)
    message(STATUS "Tray icon disabled")
endif()

if(${SUNSHINE_ENABLE_TRAY} AND ${SUNSHINE_TRAY} EQUAL 0 AND SUNSHINE_REQUIRE_TRAY)
    message(FATAL_ERROR "Tray icon is required")
endif()

if(${SUNSHINE_USE_LEGACY_INPUT})  # TODO: Remove this legacy option after the next stable release
    list(APPEND PLATFORM_TARGET_FILES "${CMAKE_SOURCE_DIR}/src/platform/linux/input/legacy_input.cpp")
else()
    # These need to be set before adding the inputtino subdirectory in order for them to be picked up
    set(LIBEVDEV_CUSTOM_INCLUDE_DIR "${EVDEV_INCLUDE_DIR}")
    set(LIBEVDEV_CUSTOM_LIBRARY "${EVDEV_LIBRARY}")

    add_subdirectory("${CMAKE_SOURCE_DIR}/third-party/inputtino")
    list(APPEND SUNSHINE_EXTERNAL_LIBRARIES inputtino::libinputtino)
    file(GLOB_RECURSE INPUTTINO_SOURCES
            ${CMAKE_SOURCE_DIR}/src/platform/linux/input/inputtino*.h
            ${CMAKE_SOURCE_DIR}/src/platform/linux/input/inputtino*.cpp)
    list(APPEND PLATFORM_TARGET_FILES ${INPUTTINO_SOURCES})

    # build libevdev before the libinputtino target
    if(EXTERNAL_PROJECT_LIBEVDEV_USED)
        add_dependencies(libinputtino libevdev)
    endif()
endif()

list(APPEND PLATFORM_TARGET_FILES
        "${CMAKE_SOURCE_DIR}/src/platform/linux/publish.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/graphics.h"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/graphics.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/misc.h"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/misc.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/audio.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/display_device.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/input.cpp"
        "${CMAKE_SOURCE_DIR}/src/platform/linux/display_device.cpp"
        "${CMAKE_SOURCE_DIR}/third-party/glad/src/egl.c"
        "${CMAKE_SOURCE_DIR}/third-party/glad/src/gl.c"
        "${CMAKE_SOURCE_DIR}/third-party/glad/include/EGL/eglplatform.h"
        "${CMAKE_SOURCE_DIR}/third-party/glad/include/KHR/khrplatform.h"
        "${CMAKE_SOURCE_DIR}/third-party/glad/include/glad/gl.h"
        "${CMAKE_SOURCE_DIR}/third-party/glad/include/glad/egl.h")

list(APPEND PLATFORM_LIBRARIES
        dl
        pulse
        pulse-simple)

include_directories(
        SYSTEM
        "${CMAKE_SOURCE_DIR}/third-party/glad/include"
        "${CMAKE_BINARY_DIR}/generated-src")
