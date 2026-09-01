cmake_minimum_required(VERSION 3.8)

add_library(LibSunvoltum SHARED
    "SunvoltumEngine.cpp"
    "SunvoltumEngine.h"
    "LibSunvoltum.h"

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

    # Input
    "Input/KeyCode.h"
    "Input/MouseButton.h"
    "Input/SunvoltumInput.h"
    "Input/SunvoltumInput.cpp"

    # Rendering
    "Rendering/RenderBridge.h"
    "Rendering/Bridge/BridgeCommon.h"
    "Rendering/Bridge/Init.cpp"
    "Rendering/Bridge/Frame.cpp"
    "Rendering/Bridge/SyncCamera.cpp"
    "Rendering/Bridge/SyncLighting.cpp"
    "Rendering/Bridge/SyncScene.cpp"
    "Rendering/Bridge/SyncSkyBox.cpp"

    # DataModel
    "DataModel/InstanceParent.h"
    "DataModel/InstanceParent.cpp"
    "DataModel/DataModel.h"
    "DataModel/DataModel.cpp"
    "DataModel/PropertyId.h"
    "DataModel/PropertyValue.h"
    "DataModel/Instance.h"
    "DataModel/Instance.cpp"
    "DataModel/PropertyManager.h"
    "DataModel/PropertyManager.cpp"

    # InstanceClasses
    "DataModel/InstanceClasses/Workspace.h"
    "DataModel/InstanceClasses/CurrentCamera.h"
    "DataModel/InstanceClasses/ShapePart.h"
    "DataModel/InstanceClasses/Lighting.h"
    "DataModel/InstanceClasses/Players.h"
    "DataModel/InstanceClasses/Decal.h"
    "DataModel/InstanceClasses/TextureSurface.h"
    "DataModel/InstanceClasses/Sound.h"
    "DataModel/InstanceClasses/Script.h"
    "DataModel/InstanceClasses/LocalScript.h"
    "DataModel/InstanceClasses/Folder.h"
    "DataModel/InstanceClasses/Model.h"
    "DataModel/InstanceClasses/Model.cpp"
    "DataModel/InstanceClasses/Motor6D.h"
    "DataModel/InstanceClasses/Motor6D.cpp"

    # Scripting
    "Scripting/ScriptBridge.h"
    "Scripting/ScriptBridge.cpp"

    # Scripting/Shared
    "Scripting/Shared/LuauHelpers.h"
    "Scripting/Shared/LuauHelpers.cpp"
    "Scripting/Shared/SharedBindings.h"
    "Scripting/Shared/SharedBindings.cpp"

    # Scripting/ServerSide
    "Scripting/ServerSide/LuauBindings.h"
    "Scripting/ServerSide/LuauBindings.cpp"
    "Scripting/ServerSide/ServerScriptBridge.h"
    "Scripting/ServerSide/ServerScriptBridge.cpp"

    # Scripting/ClientSide
    "Scripting/ClientSide/ClientScriptBridge.h"

    # SoundEngine
    "SoundEngine/SoundEngine.h"
    "SoundEngine/SoundEngine.cpp"

    # Formats
    "Formats/SMesh.h"
    "Formats/SMesh.cpp"
    "Formats/SWorld.h"
    "Formats/SWorld.cpp"

    # Shapes
    "Shapes/Block.h"
    "Shapes/Sphere.h"
    "Shapes/Cylinder.h"

    # Physics
    "Physics/PhysicsCommon.h"
    "Physics/PhysicsManager.h"
    "Physics/PhysicsManager.cpp"
    "Physics/PhysicsWorld.h"
    "Physics/PhysicsWorld.cpp"
    "Physics/PhysicsBody.h"
    "Physics/PhysicsBody.cpp"
    "Physics/PhysicsBridge.h"
    "Physics/PhysicsBridge.cpp"
)

set_target_properties(LibSunvoltum PROPERTIES OUTPUT_NAME "LibSunvoltum")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET LibSunvoltum PROPERTY CXX_STANDARD 17)
endif()

target_compile_definitions(LibSunvoltum PRIVATE SUNOVER_BUILD_DLL)
target_include_directories(LibSunvoltum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


set(METURM_ROOT "C:/MediaWork/Sunvoltum/Internal/MeturmRender")

target_include_directories(LibSunvoltum PRIVATE "${METURM_ROOT}")
target_link_libraries(LibSunvoltum PRIVATE OpenAL::OpenAL)
target_link_libraries(LibSunvoltum PRIVATE unofficial::omniverse-physx-sdk::sdk)
target_link_libraries(LibSunvoltum PRIVATE Vorbis::vorbis Vorbis::vorbisfile)
target_link_libraries(LibSunvoltum PRIVATE unofficial::luau::Luau.Compiler unofficial::luau::Luau.VM)

target_link_libraries(LibSunvoltum PRIVATE
    "$<$<CONFIG:Debug>:${METURM_ROOT}/out/build/x64-Debug/MeturmRender/MeturmRender.lib>"
    "$<$<CONFIG:Debug>:${METURM_ROOT}/out/build/x64-Debug/MeturmFrame/MeturmFrame.lib>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${METURM_ROOT}/out/build/x64-Release/MeturmRender/MeturmRender.lib>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:${METURM_ROOT}/out/build/x64-Release/MeturmFrame/MeturmFrame.lib>"
)
