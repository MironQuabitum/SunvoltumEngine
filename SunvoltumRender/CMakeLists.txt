cmake_minimum_required(VERSION 3.8)

set(METURM_ROOT "C:/MediaWork/Sunvoltum/Internal/MeturmRender")

add_library(LibSunvoltumRender SHARED
    "LibSunvoltumRender.h"

    # Rendering bridge — все исходники из SunvoltumEngine/Rendering/
    "../SunvoltumEngine/Rendering/RenderBridge.h"
    "../SunvoltumEngine/Rendering/Bridge/BridgeCommon.h"
    "../SunvoltumEngine/Rendering/Bridge/Init.cpp"
    "../SunvoltumEngine/Rendering/Bridge/Frame.cpp"
    "../SunvoltumEngine/Rendering/Bridge/SyncCamera.cpp"
    "../SunvoltumEngine/Rendering/Bridge/SyncLighting.cpp"
    "../SunvoltumEngine/Rendering/Bridge/SyncScene.cpp"
    "../SunvoltumEngine/Rendering/Bridge/SyncSkyBox.cpp"
    "../SunvoltumEngine/Rendering/Bridge/WindowUtils.cpp"

    # SunvoltumInput — адаптер поверх MeturmFrame, компилируется здесь
    "../SunvoltumEngine/Input/SunvoltumInput.cpp"

    # Runtime — игровой цикл, зависит от RenderBridge и SunvoltumInput
    "../SunvoltumEngine/Runtime/Runtime.h"
    "../SunvoltumEngine/Runtime/Runtime.cpp"
)

set_target_properties(LibSunvoltumRender PROPERTIES OUTPUT_NAME "LibSunvoltumRender")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET LibSunvoltumRender PROPERTY CXX_STANDARD 17)
endif()

# Экспортный макрос — только при сборке этой DLL
target_compile_definitions(LibSunvoltumRender PRIVATE SUNOVER_RENDER_BUILD_DLL)

# Заголовки: свои + движок (для DataModel, Types и т.д.)
target_include_directories(LibSunvoltumRender PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(LibSunvoltumRender PRIVATE ${CMAKE_SOURCE_DIR}/SunvoltumEngine)
target_include_directories(LibSunvoltumRender PRIVATE "${METURM_ROOT}")

# Зависимость на LibSunvoltum (DataModel, Engine, Types...)
target_link_libraries(LibSunvoltumRender PRIVATE LibSunvoltum)

# MeturmRender + MeturmFrame
target_link_libraries(LibSunvoltumRender PRIVATE
    "$<$<CONFIG:Debug>:${METURM_ROOT}/out/build/x64-Debug/MeturmRender/MeturmRender.lib>"
    "$<$<CONFIG:Debug>:${METURM_ROOT}/out/build/x64-Debug/MeturmFrame/MeturmFrame.lib>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${METURM_ROOT}/out/build/x64-Release/MeturmRender/MeturmRender.lib>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${METURM_ROOT}/out/build/x64-Release/MeturmFrame/MeturmFrame.lib>"
)
