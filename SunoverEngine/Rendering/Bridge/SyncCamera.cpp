#include "BridgeCommon.h"

namespace Sunover {

    void RenderBridge::SyncCamera()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::CurrentCamera::ClassId) continue;

            auto* cfProp = inst->GetProperty(Classes::CurrentCamera::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
                m_camera->SetCFrame(ToCFrame(cfProp->Value.AsCFrame));

            auto* fovProp = inst->GetProperty(Classes::CurrentCamera::FieldOfView);
            if (fovProp && fovProp->Type == PropertyType::Number)
            {
                float aspect = static_cast<float>(m_window->GetWidth()) /
                               static_cast<float>(m_window->GetHeight());
                m_camera->SetPerspective(static_cast<float>(fovProp->Value.AsNumber),
                                         aspect,
                                         m_camera->GetNearZ(),
                                         m_camera->GetFarZ());
            }
            break;
        }
    }

} // namespace Sunover
