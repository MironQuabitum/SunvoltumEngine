#pragma once

#include <algorithm>
#include <cmath>

namespace SunvoltumPhysics {

    enum class CombineMode {
        Average  = 0, // (a + b) * 0.5
        Min      = 1, // min(a, b)
        Multiply = 2, // a * b
        Max      = 3  // max(a, b)
    };

    inline float CombineParameter(float valA, float valB, CombineMode modeA, CombineMode modeB) {
        // Приоритет в PhysX: Max > Multiply > Min > Average
        CombineMode effectiveMode = (std::max)(modeA, modeB);

        switch (effectiveMode) {
            case CombineMode::Average:
                return (valA + valB) * 0.5f;
            case CombineMode::Min:
                return (std::min)(valA, valB);
            case CombineMode::Multiply:
                return valA * valB;
            case CombineMode::Max:
                return (std::max)(valA, valB);
            default:
                return (valA + valB) * 0.5f;
        }
    }

    class Material {
    public:
        Material(float staticFriction = 0.5f, float dynamicFriction = 0.4f, float restitution = 0.2f)
            : m_staticFriction(staticFriction)
            , m_dynamicFriction(dynamicFriction)
            , m_restitution(restitution) {}

        float GetStaticFriction() const { return m_staticFriction; }
        void SetStaticFriction(float v) { m_staticFriction = (std::max)(0.0f, v); }

        float GetDynamicFriction() const { return m_dynamicFriction; }
        void SetDynamicFriction(float v) { m_dynamicFriction = (std::max)(0.0f, v); }

        float GetRestitution() const { return m_restitution; }
        void SetRestitution(float v) { m_restitution = std::clamp(v, 0.0f, 1.0f); }

        CombineMode GetFrictionCombineMode() const { return m_frictionCombine; }
        void SetFrictionCombineMode(CombineMode m) { m_frictionCombine = m; }

        CombineMode GetRestitutionCombineMode() const { return m_restitutionCombine; }
        void SetRestitutionCombineMode(CombineMode m) { m_restitutionCombine = m; }

    private:
        float m_staticFriction{ 0.5f };
        float m_dynamicFriction{ 0.4f };
        float m_restitution{ 0.2f };

        CombineMode m_frictionCombine{ CombineMode::Average };
        CombineMode m_restitutionCombine{ CombineMode::Average };
    };

} // namespace SunvoltumPhysics
