#pragma once

// Общие включения и конвертеры типов Sunover → MeturmRender.
// Подключается только внутри Bridge/*.cpp

#include <MeturmRender/Renderer/Renderer.h>
#include <MeturmRender/Core/Window.h>
#include <MeturmRender/Objects/Camera.h>
#include <MeturmRender/Objects/SunLight.h>
#include <MeturmRender/Objects/SkyBox.h>
#include <MeturmRender/Objects/MeshObject.h>
#include <MeturmRender/Objects/Cursor.h>
#include <MeturmRender/Interface/RenderTypes.h>
#include <fstream>
#include <MeturmRender/Types/CFrame.h>
#include <MeturmRender/Types/Matrix3x3.h>
#include <MeturmRender/Types/Mesh.h>

#include "../RenderBridge.h"
#include "../../Core/Engine.h"
#include "../../DataModel/InstanceClasses/ShapePart.h"
#include "../../DataModel/InstanceClasses/CurrentCamera.h"
#include "../../DataModel/InstanceClasses/Lighting.h"
#include "../../DataModel/InstanceClasses/Decal.h"
#include "../../DataModel/InstanceClasses/TextureSurface.h"
#include "../../Shapes/Block.h"
#include "../../Shapes/Sphere.h"
#include "../../Shapes/Cylinder.h"

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

    // Конвертер Sunover::Mesh → MeturmRender::Types::Mesh
    inline MeturmRender::Types::Mesh ToRenderMesh(const Sunover::Mesh& mesh)
    {
        std::vector<MeturmRender::Types::Vertex> verts;
        verts.reserve(mesh.Vertices.size());

        for (const auto& v : mesh.Vertices)
        {
            MeturmRender::Types::Vertex rv;
            rv.position = ToRender3(v.Position);
            rv.normal   = ToRender3(v.Normal);
            rv.uv       = { v.UV.X, v.UV.Y };
            rv.color    = { v.Color.R, v.Color.G, v.Color.B, 1.0f };
            verts.push_back(rv);
        }

        return MeturmRender::Types::Mesh(verts, mesh.Indices);
    }

} // namespace Sunover
