cmake_minimum_required(VERSION 3.16)
project(RoomverbSimulation)
include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


add_executable(${PROJECT_NAME}
src/main.c  "src/raytracer.c" "src/audio.c" "src/audio.h" "src/ui.c" "src/roomverb.c")

set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resources)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_DIR}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)

# raylib
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)

# raygui
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
)


# Optional raylib settings
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib raygui)

# cglm
add_subdirectory(vendor/cglm EXCLUDE_FROM_ALL)

#nfd




if(WIN32)
    set(NFD_PLATFORM_SRC vendor/nativefiledialog/src/nfd_win.cpp)
elseif(APPLE)
    set(NFD_PLATFORM_SRC vendor/nativefiledialog/src/nfd_cocoa.m)
else()
    set(NFD_PLATFORM_SRC vendor/nativefiledialog/src/nfd_gtk.c)
endif()

add_library(nfd STATIC
    vendor/nativefiledialog/src/nfd_common.c
    ${NFD_PLATFORM_SRC}
)

target_include_directories(nfd PUBLIC
    vendor/nativefiledialog/src/include
)

#raygizmo
add_library(raylib-gizmo STATIC
    vendor/raylib-gizmo/src/raygizmo.c
)

target_include_directories(raylib-gizmo PUBLIC
    vendor/raylib-gizmo/src
    
)
target_link_libraries(raylib-gizmo PUBLIC raylib)


#raylib blender camera
add_library(raylib-blender-camera INTERFACE)

target_include_directories(raylib-blender-camera INTERFACE
    ${CMAKE_SOURCE_DIR}/vendor/raylib-blender-camera/src
)

target_link_libraries(raylib-blender-camera INTERFACE
    raylib
)



# ffmpeg
set(FFMPEG_DIR ${CMAKE_SOURCE_DIR}/vendor/ffmpeg)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)


target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${raygui_SOURCE_DIR}/src
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${FFMPEG_LIB_DIR}
)

# openmp
find_package(OpenMP REQUIRED)



target_link_libraries(${PROJECT_NAME}
    PRIVATE
    cglm_headers
    nfd
    raylib-gizmo
    raylib-blender-camera
    raylib
    avcodec
    avformat
    avutil
    swresample
    OpenMP::OpenMP_C
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${FFMPEG_DIR}/bin
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)