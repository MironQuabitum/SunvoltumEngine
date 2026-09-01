#include "BridgeCommon.h"

namespace Sunvoltum {

    // -------------------------------------------------------------------------
    // SubscribeCamera — вызывается один раз из Init.
    // Кэшируем указатель на CurrentCamera Instance и подписываемся на
    // CFrame и FieldOfView. Коллбэки применяют изменение к MeturmRender::Camera
    // немедленно и выставляют m_needCameraSync=true для синхронизации.
    // -------------------------------------------------------------------------
    void RenderBridge::SubscribeCamera()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::CurrentCamera::ClassId) continue;
            m_cameraInst = inst.get();
            break;
        }
        if (!m_cameraInst) return;

        auto& pm = PropertyManager::Get();

        // CFrame меняется каждый кадр (орбита/физика) — коллбэк применяет сразу
        m_camCFrameToken = pm.Subscribe(m_cameraInst, Classes::CurrentCamera::CFrame,
            [this](const PropertyValue& val)
            {
                if (val.Type != PropertyType::CFrame) return;
                m_camera->SetCFrame(ToCFrame(val.Value.AsCFrame));
            });

        // FOV меняется редко — коллбэк обновляет матрицу проекции немедленно
        m_camFovToken = pm.Subscribe(m_cameraInst, Classes::CurrentCamera::FieldOfView,
            [this](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Number) return;
                float aspect = static_cast<float>(m_window->GetWidth()) /
                               static_cast<float>(m_window->GetHeight());
                m_camera->SetPerspective(static_cast<float>(val.Value.AsNumber),
                                         aspect,
                                         m_camera->GetNearZ(),
                                         m_camera->GetFarZ());
            });

        // Применяем начальные значения
        auto* cf = m_cameraInst->GetProperty(Classes::CurrentCamera::CFrame);
        if (cf && cf->Type == PropertyType::CFrame)
            m_camera->SetCFrame(ToCFrame(cf->Value.AsCFrame));

        auto* fov = m_cameraInst->GetProperty(Classes::CurrentCamera::FieldOfView);
        if (fov && fov->Type == PropertyType::Number)
        {
            float aspect = static_cast<float>(m_window->GetWidth()) /
                           static_cast<float>(m_window->GetHeight());
            m_camera->SetPerspective(static_cast<float>(fov->Value.AsNumber),
                                     aspect,
                                     m_camera->GetNearZ(),
                                     m_camera->GetFarZ());
        }

        m_needCameraSync = false;
    }

    // -------------------------------------------------------------------------
    // SyncCamera — вызывается каждый кадр из Frame().
    // Теперь ничего не делает: CFrame и FOV обновляются мгновенно через коллбэки.
    // Метод оставлен как точка расширения (resize aspect ratio и т.п.).
    // -------------------------------------------------------------------------
    void RenderBridge::SyncCamera()
    {
        // Всё обновляется через PropertyManager callbacks в SubscribeCamera().
        // Resize aspect обрабатывается в PollEvents() → уже там.
    }

} // namespace Sunvoltum
