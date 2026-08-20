#include "BridgeCommon.h"

namespace Sunover {

    void RenderBridge::SyncScene()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::ShapePart::ClassId) continue;

            MeturmRender::Objects::MeshObject meshObj;

            auto* cfProp = inst->GetProperty(Classes::ShapePart::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
                meshObj.SetCFrame(ToCFrame(cfProp->Value.AsCFrame));

            auto* transProp = inst->GetProperty(Classes::ShapePart::Transparency);
            if (transProp && transProp->Type == PropertyType::Float)
                meshObj.SetTransparency(transProp->Value.AsFloat);

            m_renderer->RenderObject(meshObj, *m_camera);
        }
    }

} // namespace Sunover
