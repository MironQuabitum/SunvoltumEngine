cmake_minimum_required(VERSION 3.8)

set(SUNVOLTUM_RENDER_ROOT "${CMAKE_SOURCE_DIR}/../SunvoltumRender")

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

    # SunvoltumInput — адаптер поверх SunvoltumManager, компилируется здесь
    "../SunvoltumEngine/Input/SunvoltumInput.cpp"
)

set_target_properties(LibSunvoltumRender PROPERTIES OUTPUT_NAME "LibSunvoltumRender")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET LibSunvoltumRender PROPERTY CXX_STANDARD 20)
endif()

# Экспортный макрос — только при сборке этой DLL
target_compile_definitions(LibSunvoltumRender PRIVATE SUNOVER_RENDER_BUILD_DLL)

# Заголовки: свои + движок (для DataModel, Types и т.д.) + SunvoltumRender
target_include_directories(LibSunvoltumRender PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(LibSunvoltumRender PRIVATE ${CMAKE_SOURCE_DIR}/SunvoltumEngine)
target_include_directories(LibSunvoltumRender PRIVATE "${SUNVOLTUM_RENDER_ROOT}")

# Зависимость на LibSunvoltum (DataModel, Engine, Types...)
target_link_libraries(LibSunvoltumRender PRIVATE LibSunvoltum)

# Линковка с SunvoltumRender, SunvoltumManager, SunvoltumD3D9
target_link_libraries(LibSunvoltumRender PRIVATE
    "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumRender/SunvoltumRender.lib,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumRender/SunvoltumRender.lib>"
    "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumManager/SunvoltumManager.lib,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumManager/SunvoltumManager.lib>"
    "$<IF:$<CONFIG:Debug>,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Debug/SunvoltumD3D9/SunvoltumD3D9.lib,${SUNVOLTUM_RENDER_ROOT}/out/build/x64-Release/SunvoltumD3D9/SunvoltumD3D9.lib>"
    d3d9
    d3dcompiler
)
