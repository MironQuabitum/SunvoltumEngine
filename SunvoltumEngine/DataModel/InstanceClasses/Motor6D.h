#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"
#include "../../Types/CFrame.h"
#include "../../LibSunvoltum.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_MOTOR6D = 13;

    // Motor6D -- rigid joint between two ShapeParts (analog of Roblox Motor6D).
    //
    // Each frame PhysicsBridge computes Part1 world CFrame as:
    //   worldCF1 = Part0.CFrame * C0 * Inverse(C1)
    //
    //   C0 -- attachment offset in Part0 local space
    //   C1 -- attachment offset in Part1 local space (usually Identity)
    //
    // To animate, change C0 each frame:
    //   motor.C0 = CFrame(offset) * CFrame::Angles(pitch, yaw, roll)
    struct Motor6D
    {
        static constexpr int8_t ClassId = CLASS_MOTOR6D;

        // Reference to the anchor part (InstanceRef -> ShapePart).
        static constexpr PropertyId Part0 = 0; // InstanceRef

        // Reference to the driven part (InstanceRef -> ShapePart).
        // Positioned each frame as: Part0.CFrame * C0 * Inv(C1)
        static constexpr PropertyId Part1 = 1; // InstanceRef

        // Attachment offset in Part0 local space. Default: Identity.
        static constexpr PropertyId C0 = 2; // CFrame

        // Attachment offset in Part1 local space. Default: Identity.
        static constexpr PropertyId C1 = 3; // CFrame

        // Initialize default properties on inst.
        // Must be called after AddInstance("...", Motor6D::ClassId).
        // Exported from DLL via LibSunvoltum.
        LibSunvoltum static void Init(Instance& inst);

        static void SetParts(Instance& inst, Instance* part0, Instance* part1)
        {
            inst.SetProperty(Part0, PropertyValue::Ref(part0));
            inst.SetProperty(Part1, PropertyValue::Ref(part1));
        }

        static void SetC0(Instance& inst, const CFrame& c0)
        {
            inst.SetProperty(C0, PropertyValue::CFrame(c0));
        }

        static void SetC1(Instance& inst, const CFrame& c1)
        {
            inst.SetProperty(C1, PropertyValue::CFrame(c1));
        }
    };

} // namespace Classes
} // namespace Sunvoltum
