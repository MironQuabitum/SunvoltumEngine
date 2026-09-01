#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Types/CFrame.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_MODEL = 12;

    // Model — контейнер для группы объектов.
    // Поддерживает операции перемещения всей модели через PivotTo
    // и SetPrimaryPartCFrame (относительно PrimaryPart).
    struct Model
    {
        static constexpr int8_t ClassId = CLASS_MODEL;

        // Ссылка на основную часть модели (Instance*, слабый указатель).
        // Используется в SetPrimaryPartCFrame для вычисления смещений.
        static constexpr PropertyId PrimaryPart = 0; // InstanceRef

        // Получить PrimaryPart из экземпляра
        static Instance* GetPrimaryPart(const Instance& inst)
        {
            const PropertyValue* pv = inst.GetProperty(PrimaryPart);
            if (pv && pv->Type == PropertyType::InstanceRef)
                return pv->Value.AsInstanceRef;
            return nullptr;
        }

        // PivotTo(model, targetCFrame)
        // Перемещает всю модель так, чтобы её «pivot» (среднее положение дочерних
        // ShapePart-ов, или позиция PrimaryPart если задан) совпал с targetCFrame.
        // Все дочерние ShapePart перемещаются с сохранением относительного смещения.
        static void PivotTo(Instance& model, const CFrame& targetCFrame);

        // SetPrimaryPartCFrame(model, targetCFrame)
        // Перемещает всю модель так, чтобы PrimaryPart оказался в targetCFrame.
        // Требует назначенного PrimaryPart; если его нет — ничего не делает.
        static void SetPrimaryPartCFrame(Instance& model, const CFrame& targetCFrame);
    };

} // namespace Classes
} // namespace Sunvoltum
