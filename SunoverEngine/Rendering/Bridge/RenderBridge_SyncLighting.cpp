#include "BridgeCommon.h"

namespace Sunover {

    void RenderBridge::SyncLighting()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::Lighting::ClassId) continue;

            auto* brightProp = inst->GetProperty(Classes::Lighting::Brightness);
            if (brightProp && brightProp->Type == PropertyType::Number)
                m_sunLight->SetIntensity(static_cast<float>(brightProp->Value.AsNumber));

            break;
        }
    }

} // namespace Sunover
