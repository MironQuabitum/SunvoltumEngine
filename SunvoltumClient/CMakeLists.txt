cmake_minimum_required(VERSION 3.8)

add_executable(SunvoltumClient
    "main.cpp"
    "App/ClientApp.h"
    "App/ClientApp.cpp"
    "Camera/ClientCameraController.h"
    "Camera/ClientCameraController.cpp"
    "Environment/ClientSceneInitializer.h"
    "Environment/ClientSceneInitializer.cpp"
    "Network/ClientNetworkController.h"
    "Network/ClientNetworkController.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET SunvoltumClient PROPERTY CXX_STANDARD 17)
endif()

# Подключаем заголовки LibSunvoltum и LibSunvoltumRender
target_include_directories(SunvoltumClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/SunvoltumEngine
    ${CMAKE_SOURCE_DIR}/SunvoltumRender
)

# Линкуемся с LibSunvoltum.lib и LibSunvoltumRender.lib
target_link_libraries(SunvoltumClient PRIVATE LibSunvoltum LibSunvoltumRender)

# После сборки — копируем LibSunvoltum.dll рядом с .exe
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibSunvoltum>
        $<TARGET_FILE_DIR:SunvoltumClient>
)

# Копируем LibSunvoltumRender.dll рядом с .exe
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibSunvoltumRender>
        $<TARGET_FILE_DIR:SunvoltumClient>
)

# Копируем SunvoltumRender.dll рядом с .exe
set(SUNVOLTUM_RENDER_ROOT "${CMAKE_SOURCE_DIR}/../SunvoltumRender")
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumRender/SunvoltumRender.dll,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumRender/SunvoltumRender.dll>"
        $<TARGET_FILE_DIR:SunvoltumClient>
)

# Копируем PlatformContent рядом с .exe
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/PlatformContent
        $<TARGET_FILE_DIR:SunvoltumClient>/PlatformContent
)

# Копируем LocalScript-ы рядом с .exe
add_custom_command(TARGET SunvoltumClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Scripts
        $<TARGET_FILE_DIR:SunvoltumClient>/Scripts
)
