#pragma once

// Общие включения и конвертеры типов Sunover → MeturmRender.
// Подключается только внутри Bridge/*.cpp

#include <MeturmRender/Renderer/Renderer.h>
#include <MeturmRender/Core/Window.h>
#include <MeturmRender/Objects/Camera.h>
#include <MeturmRender/Objects/SunLight.h>
#include <MeturmRender/Objects/MeshObject.h>
#include <MeturmRender/Interface/RenderTypes.h>
#include <MeturmRender/Types/CFrame.h>
#include <MeturmRender/Types/Matrix3x3.h>

#include "../RenderBridge.h"
#include "../../Core/Engine.h"
#include "../../DataModel/InstanceClasses/ShapePart.h"
#include "../../DataModel/InstanceClasses/CurrentCamera.h"
#include "../../DataModel/InstanceClasses/Lighting.h"

namespace Sunover {

    inline MeturmRender::Types::Render3 ToRender3(const Vector3& v)
    {
        return MeturmRender::Types::Render3(v.X, v.Y, v.Z);
    }

    inline MeturmRender::Types::Matrix3x3 ToMatrix(const Matrix3x3& m)
    {
        return MeturmRender::Types::Matrix3x3(
            m.R00, m.R01, m.R02,
            m.R10, m.R11, m.R12,
            m.R20, m.R21, m.R22
        );
    }

    inline MeturmRender::Types::CFrame ToCFrame(const CFrame& cf)
    {
        return MeturmRender::Types::CFrame(ToRender3(cf.Position), ToMatrix(cf.Rotation));
    }

} // namespace Sunover
