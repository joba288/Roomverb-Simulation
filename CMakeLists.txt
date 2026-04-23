cmake_minimum_required(VERSION 3.16)
project(RoomverbSimulation)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


add_executable(${PROJECT_NAME}
src/main.c )

add_subdirectory(vendor/cglm EXCLUDE_FROM_ALL)

set(FFMPEG_DIR ${CMAKE_SOURCE_DIR}/vendor/ffmpeg)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${FFMPEG_INCLUDE_DIR}
)

target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${FFMPEG_LIB_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    cglm_headers
    avcodec
    avformat
    avutil
    swresample
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${FFMPEG_DIR}/bin
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)