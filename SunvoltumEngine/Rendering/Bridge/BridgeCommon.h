#pragma once

// Общие включения и конвертеры типов Sunvoltum → SunvoltumRender.
// Подключается только внутри Bridge/*.cpp

#include <SunvoltumRender/SunvoltumRender.h>
#include <SunvoltumManager/WindowManager.h>
#include <SunvoltumManager/Interfaces/IPlatformWindow.h>
#include <fstream>

#include "../RenderBridge.h"
#include "../../Core/Engine.h"
#include "../../DataModel/InstanceClasses/ShapePart.h"
#include "../../DataModel/InstanceClasses/CurrentCamera.h"
#include "../../DataModel/InstanceClasses/Lighting.h"
#include "../../DataModel/InstanceClasses/Decal.h"
#include "../../DataModel/InstanceClasses/TextureSurface.h"
#include "../../DataModel/PropertyManager.h"
#include "../../Shapes/Block.h"
#include "../../Shapes/Sphere.h"
#include "../../Shapes/Cylinder.h"

namespace Sunvoltum {

    inline SunvoltumRender::Types::Vector3 ToRender3(const Vector3& v)
    {
        return SunvoltumRender::Types::Vector3(v.X, v.Y, v.Z);
    }

    inline SunvoltumRender::Types::Matrix3x3 ToMatrix(const Matrix3x3& m)
    {
        return SunvoltumRender::Types::Matrix3x3(
            m.R00, m.R01, m.R02,
            m.R10, m.R11, m.R12,
            m.R20, m.R21, m.R22
        );
    }

    // Конвертер Sunvoltum::Mesh → SunvoltumRender::Types::Mesh
    inline SunvoltumRender::Types::Mesh ToRenderMesh(const Sunvoltum::Mesh& mesh)
    {
        std::vector<SunvoltumRender::Types::Vertex> verts;
        verts.reserve(mesh.Vertices.size());

        for (const auto& v : mesh.Vertices)
        {
            SunvoltumRender::Types::Vertex rv;
            rv.position = ToRender3(v.Position);
            rv.normal   = ToRender3(v.Normal);
            rv.uv       = { v.UV.X, v.UV.Y };
            rv.color    = { v.Color.R, v.Color.G, v.Color.B, 1.0f };
            verts.push_back(rv);
        }

        return SunvoltumRender::Types::Mesh(verts, mesh.Indices);
    }

} // namespace Sunvoltum
