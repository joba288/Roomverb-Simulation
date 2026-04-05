cmake_minimum_required(VERSION 3.16)
project(RoomverbSimulation)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


add_executable(${PROJECT_NAME}
src/main.c )

add_subdirectory(vendor/cglm EXCLUDE_FROM_ALL)
target_link_libraries(RoomverbSimulation PRIVATE cglm_headers)




#target_include_directories(${PROJECT_NAME} PRIVATE vendor/ffmpeg/include)
#target_link_directories(${PROJECT_NAME} PRIVATE vendor/ffmpeg/lib)
#target_link_libraries(${PROJECT_NAME} PRIVATE avformat avcodec avutil)
#
#add_custom_command(
#        TARGET ${PROJECT_NAME} POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_SOURCE_DIR}/vendor/ffmpeg/bin
#        ${CMAKE_CURRENT_BINARY_DIR}
#)
#
#add_custom_command(
#    TARGET ${PROJECT_NAME} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#            ${CMAKE_SOURCE_DIR}/resources
#            ${CMAKE_CURRENT_BINARY_DIR}/resources
#)