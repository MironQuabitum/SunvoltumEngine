#include "Motor6D.h"
#include "../PropertyValue.h"

namespace Sunvoltum {
namespace Classes {

    void Motor6D::Init(Instance& inst)
    {
        // Part0 и Part1 — пустые ссылки до момента явного назначения
        inst.SetProperty(Part0, PropertyValue::Ref(nullptr));
        inst.SetProperty(Part1, PropertyValue::Ref(nullptr));

        // C0 и C1 — Identity (нет смещения, часть крепится к центру)
        inst.SetProperty(C0, PropertyValue::CFrame(CFrame::FromPosition(0.0f, 0.0f, 0.0f)));
        inst.SetProperty(C1, PropertyValue::CFrame(CFrame::FromPosition(0.0f, 0.0f, 0.0f)));
    }

} // namespace Classes
} // namespace Sunvoltum
