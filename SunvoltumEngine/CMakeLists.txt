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
    "Types/CFrameUtils.h"
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
    # SunvoltumInput.cpp компилируется в LibSunvoltumRender (зависит от SunvoltumManager)

    # Rendering (только заголовок — реализация в LibSunvoltumRender.dll)
    "Rendering/RenderBridge.h"

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
    "DataModel/InstanceRegistry.h"
    "DataModel/InstanceRegistry.cpp"

    # InstanceClasses
    "DataModel/InstanceClasses/BasePart.h"
    "DataModel/InstanceClasses/JointInstance.h"
    "DataModel/InstanceClasses/Weld.h"
    "DataModel/InstanceClasses/Workspace.h"
    "DataModel/InstanceClasses/CurrentCamera.h"
    "DataModel/InstanceClasses/ShapePart.h"
    "DataModel/InstanceClasses/Lighting.h"
    "DataModel/InstanceClasses/Players.h"
    "DataModel/InstanceClasses/Player.h"
    "DataModel/InstanceClasses/Decal.h"
    "DataModel/InstanceClasses/TextureSurface.h"
    "DataModel/InstanceClasses/Sound.h"
    "DataModel/InstanceClasses/Script.h"
    "DataModel/InstanceClasses/LocalScript.h"
    "DataModel/InstanceClasses/Folder.h"
    "DataModel/InstanceClasses/Model.h"
    "DataModel/InstanceClasses/Model.cpp"

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
    "Scripting/ClientSide/ClientScriptBridge.cpp"
    "Scripting/ClientSide/ClientBindings.h"
    "Scripting/ClientSide/ClientBindings.cpp"
    "Scripting/ClientSide/ClientInstanceBindings.h"
    "Scripting/ClientSide/ClientInstanceBindings.cpp"

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

    # Network
    "Network/NetworkPacket.h"
    "Network/NetworkManager.h"
    "Network/NetworkManager.cpp"
    "Network/NetworkServer.h"
    "Network/NetworkServer.cpp"
    "Network/NetworkClient.h"
    "Network/NetworkClient.cpp"
    "Network/NetworkSerializer.h"
    "Network/NetworkSerializer.cpp"
    "Network/SceneSerializer.h"
    "Network/SceneSerializer.cpp"
    "Network/SceneDeserializer.h"
    "Network/SceneDeserializer.cpp"
    "Network/ServerReplicator.h"
    "Network/ServerReplicator.cpp"
    "Network/ClientReplicator.h"
    "Network/ClientReplicator.cpp"

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
target_include_directories(LibSunvoltum PRIVATE ${CMAKE_SOURCE_DIR}/SunvoltumRender)


target_link_libraries(LibSunvoltum PRIVATE OpenAL::OpenAL)
target_link_libraries(LibSunvoltum PRIVATE unofficial::omniverse-physx-sdk::sdk)
target_link_libraries(LibSunvoltum PRIVATE Vorbis::vorbis Vorbis::vorbisfile)
target_link_libraries(LibSunvoltum PRIVATE unofficial::luau::Luau.Compiler unofficial::luau::Luau.VM)
target_link_libraries(LibSunvoltum PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)