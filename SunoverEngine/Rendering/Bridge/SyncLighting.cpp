#include "BridgeCommon.h"
#include <cmath>
#include <algorithm>

namespace Sunover {

    static constexpr float PI      = 3.14159265358979323846f;
    static constexpr float DEG2RAD = PI / 180.0f;

    static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    static void LerpColor(float t,
                          float r0, float g0, float b0,
                          float r1, float g1, float b1,
                          float& r,  float& g,  float& b)
    {
        r = r0 + (r1 - r0) * t;
        g = g0 + (g1 - g0) * t;
        b = b0 + (b1 - b0) * t;
    }

    void RenderBridge::SyncLighting()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::Lighting::ClassId) continue;

            // --- Читаем параметры ---
            float clockTime  = 14.0f;
            float latitude   = 45.0f;
            float brightness = 1.0f;

            auto* ctProp = inst->GetProperty(Classes::Lighting::ClockTime);
            if (ctProp && ctProp->Type == PropertyType::Number)
                clockTime = static_cast<float>(ctProp->Value.AsNumber);

            auto* latProp = inst->GetProperty(Classes::Lighting::GeographicLatitude);
            if (latProp && latProp->Type == PropertyType::Number)
                latitude = static_cast<float>(latProp->Value.AsNumber);

            auto* brightProp = inst->GetProperty(Classes::Lighting::Brightness);
            if (brightProp && brightProp->Type == PropertyType::Number)
                brightness = static_cast<float>(brightProp->Value.AsNumber);

            // --- Расчёт положения солнца ---
            // Часовой угол: полдень = 0, +1ч = PI/12 рад
            const float H   = (clockTime - 12.0f) * (PI / 12.0f);
            const float lat = latitude * DEG2RAD;
            const float decl = 0.0f; // равноденствие

            // Высота солнца над горизонтом
            const float sinAlt   = std::sinf(lat) * std::sinf(decl)
                                 + std::cosf(lat) * std::cosf(decl) * std::cosf(H);
            const float altitude = std::asinf(ClampF(sinAlt, -1.0f, 1.0f));

            // Азимут
            const float cosAlt = std::cosf(altitude);
            const float cosAz  = (std::sinf(decl) - std::sinf(altitude) * std::sinf(lat))
                                / (cosAlt * std::cosf(lat) + 1e-6f);
            const float azimuth = std::acosf(ClampF(cosAz, -1.0f, 1.0f))
                                * (std::sinf(H) > 0.0f ? 1.0f : -1.0f);

            // --- CFrame для SunLight ---
            MeturmRender::Types::CFrame sunCF = ToCFrame(
                Sunover::CFrame(
                    Vector3(std::cosf(altitude) * std::sinf(azimuth) * 500.0f,
                            std::sinf(altitude) * 500.0f,
                            std::cosf(altitude) * std::cosf(azimuth) * 500.0f),
                    Matrix3x3::FromEuler(altitude, azimuth, 0.0f)
                )
            );
            m_sunLight->SetCFrame(sunCF);

            // --- Интенсивность ---
            const float sunHeight01 = Clamp01(altitude / (PI * 0.5f));
            m_sunLight->SetIntensity(sunHeight01 * brightness);

            // --- Цвет солнца ---
            float sr, sg, sb;
            if (sunHeight01 < 0.08f)
            {
                float t = Clamp01(sunHeight01 / 0.08f);
                LerpColor(t,
                    1.0f, 0.2f, 0.05f,
                    1.0f, 0.6f, 0.3f,
                    sr, sg, sb);
            }
            else if (sunHeight01 < 0.25f)
            {
                float t = Clamp01((sunHeight01 - 0.08f) / 0.17f);
                LerpColor(t,
                    1.0f, 0.6f,  0.3f,
                    1.0f, 0.95f, 0.85f,
                    sr, sg, sb);
            }
            else
            {
                sr = 1.0f; sg = 0.98f; sb = 0.95f;
            }
            m_sunLight->SetColor(sr, sg, sb);

            // --- Ambient ---
            float ambientT = Clamp01(sunHeight01 * 4.0f);
            float ar, ag, ab;
            LerpColor(ambientT,
                0.02f, 0.02f, 0.05f,
                0.3f,  0.4f,  0.6f,
                ar, ag, ab);
            m_sunLight->SetAmbient(ar * brightness, ag * brightness, ab * brightness);
            m_sunLight->SetOutdoorAmbient(ar * brightness * 1.2f,
                                          ag * brightness * 1.2f,
                                          ab * brightness * 1.2f);
            break;
        }
    }

} // namespace Sunover
