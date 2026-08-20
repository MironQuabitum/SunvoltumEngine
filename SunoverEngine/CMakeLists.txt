cmake_minimum_required(VERSION 3.8)

add_library(LibSunover SHARED
    "SunoverEngine.cpp"
    "SunoverEngine.h"
    "LibSunover.h"

    # Types
    "Types/EngineMode.h"
    "Types/Shape.h"
    "Types/Color3.h"
    "Types/Vector2.h"
    "Types/Vector3.h"
    "Types/Matrix3x3.h"
    "Types/CFrame.h"
    "Types/Number.h"
    "Types/CameraType.h"
    "Types/Mesh.h"

    # Core
    "Core/Engine.h"
    "Core/Engine.cpp"

    # Runtime
    "Runtime/Runtime.h"
    "Runtime/Runtime.cpp"

    # Rendering
    "Rendering/RenderBridge.h"
    "Rendering/Bridge/BridgeCommon.h"
    "Rendering/Bridge/RenderBridge_Init.cpp"
    "Rendering/Bridge/RenderBridge_Frame.cpp"
    "Rendering/Bridge/RenderBridge_SyncCamera.cpp"
    "Rendering/Bridge/RenderBridge_SyncLighting.cpp"
    "Rendering/Bridge/RenderBridge_SyncScene.cpp"

    # DataModel
    "DataModel/InstanceParent.h"
    "DataModel/DataModel.h"
    "DataModel/DataModel.cpp"
    "DataModel/PropertyId.h"
    "DataModel/PropertyValue.h"
    "DataModel/Instance.h"
    "DataModel/Instance.cpp"

    # InstanceClasses
    "DataModel/InstanceClasses/Workspace.h"
    "DataModel/InstanceClasses/CurrentCamera.h"
    "DataModel/InstanceClasses/ShapePart.h"
    "DataModel/InstanceClasses/Lighting.h"
    "DataModel/InstanceClasses/Players.h"

    # Formats
    "Formats/SMesh.h"
    "Formats/SMesh.cpp"
    "Formats/SWorld.h"
    "Formats/SWorld.cpp"

    # Shapes
    "Shapes/Block.h"
    "Shapes/Sphere.h"
    "Shapes/Cylinder.h"
)

set_target_properties(LibSunover PROPERTIES OUTPUT_NAME "LibSunover")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET LibSunover PROPERTY CXX_STANDARD 20)
endif()

target_compile_definitions(LibSunover PRIVATE SUNOVER_BUILD_DLL)
target_include_directories(LibSunover PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MeturmRender — линкуемся к уже собранным .lib
set(METURM_ROOT "C:/MediaWork/Meturm/Internal/MeturmRender")

target_include_directories(LibSunover PRIVATE "${METURM_ROOT}")

target_link_libraries(LibSunover PRIVATE
    "$<$<CONFIG:Debug>:${METURM_ROOT}/out/build/x64-Debug/MeturmRender/MeturmRender.lib>"
    "$<$<CONFIG:Debug>:${METURM_ROOT}/out/build/x64-Debug/MeturmFrame/MeturmFrame.lib>"
    "$<$<CONFIG:Release>:${METURM_ROOT}/out/build/x64-Release/MeturmRender/MeturmRender.lib>"
    "$<$<CONFIG:Release>:${METURM_ROOT}/out/build/x64-Release/MeturmFrame/MeturmFrame.lib>"
)
