cmake_minimum_required(VERSION 3.8)

project(SunvoltumPDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SUNVOLTUM_RENDER_ROOT "${CMAKE_SOURCE_DIR}/../SunvoltumRender")

add_executable(SunvoltumPDemo
    "src/main.cpp"
)

target_include_directories(SunvoltumPDemo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/SunvoltumPhysics/include"
    "${SUNVOLTUM_RENDER_ROOT}"
)

target_link_libraries(SunvoltumPDemo PRIVATE
    SunvoltumPhysics
    "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumRender/SunvoltumRender.lib,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumRender/SunvoltumRender.lib>"
    "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumManager/SunvoltumManager.lib,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumManager/SunvoltumManager.lib>"
)

add_custom_command(TARGET SunvoltumPDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SUNVOLTUM_RENDER_ROOT}/PlatformContent"
        "$<TARGET_FILE_DIR:SunvoltumPDemo>/PlatformContent"
)

add_custom_command(TARGET SunvoltumPDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumRender/SunvoltumRender.dll,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumRender/SunvoltumRender.dll>"
        "$<TARGET_FILE_DIR:SunvoltumPDemo>"
)

add_custom_command(TARGET SunvoltumPDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/build/SunvoltumRender/Debug/SunvoltumRender.dll,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumRender/SunvoltumRender.dll>"
        "$<TARGET_FILE_DIR:SunvoltumPDemo>"
)
