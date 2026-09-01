#pragma once

// Общие заголовки PhysX 5 для всей Physics-подсистемы.
// Подключается только внутри Physics/*.cpp и Physics/*.h
//
// vcpkg (unofficial-omniverse-physx-sdk) устанавливает заголовки в:
//   <vcpkg>/installed/x64-windows/include/physx/
// и прописывает INTERFACE_INCLUDE_DIRECTORIES = .../include/physx
// Поэтому включаем без префикса: <PxPhysicsAPI.h>

#include <PxPhysicsAPI.h>

// Удобный алиас неймспейса
namespace px = physx;

// Все PhysX-объекты освобождаются через release(), а не delete.
template<typename T>
inline void PxSafeRelease(T*& ptr)
{
    if (ptr)
    {
        ptr->release();
        ptr = nullptr;
    }
}
