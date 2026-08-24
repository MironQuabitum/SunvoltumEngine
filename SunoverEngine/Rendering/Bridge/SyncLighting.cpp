#include "BridgeCommon.h"
#include "../../DataModel/InstanceClasses/CurrentCamera.h"
#include <MeturmRender/Objects/SkyBox.h>
#include <iostream>
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
        // --- Дефолтные параметры освещения (используются если Lighting не добавлен) ---
        float clockTime  = 14.0f;
        float latitude   = 45.0f;
        float brightness = 1.0f;

        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::Lighting::ClassId) continue;

            auto* ctProp = inst->GetProperty(Classes::Lighting::ClockTime);
            if (ctProp && ctProp->Type == PropertyType::Number)
                clockTime = static_cast<float>(ctProp->Value.AsNumber);

            auto* latProp = inst->GetProperty(Classes::Lighting::GeographicLatitude);
            if (latProp && latProp->Type == PropertyType::Number)
                latitude = static_cast<float>(latProp->Value.AsNumber);

            auto* brightProp = inst->GetProperty(Classes::Lighting::Brightness);
            if (brightProp && brightProp->Type == PropertyType::Number)
                brightness = static_cast<float>(brightProp->Value.AsNumber);

            break;
        }

        // --- Расчёт положения солнца ---
        const float H    = (clockTime - 12.0f) * (PI / 12.0f);
        const float lat  = latitude * DEG2RAD;
        const float decl = 0.0f;

        const float sinAlt   = std::sinf(lat) * std::sinf(decl)
                             + std::cosf(lat) * std::cosf(decl) * std::cosf(H);
        const float altitude = std::asinf(ClampF(sinAlt, -1.0f, 1.0f));

        const float cosAlt = std::cosf(altitude);
        const float cosAz  = (std::sinf(decl) - std::sinf(altitude) * std::sinf(lat))
                            / (cosAlt * std::cosf(lat) + 1e-6f);
        const float azimuth = std::acosf(ClampF(cosAz, -1.0f, 1.0f))
                            * (std::sinf(H) > 0.0f ? 1.0f : -1.0f);

        // sunHeight01: 1.0 = зенит, 0.0 = горизонт, отрицательное = под горизонтом
        // Для логики дня/ночи используем сырое значение altitude
        const float sunHeight01 = Clamp01(altitude / (PI * 0.5f));

        // --- Переход день/ночь ---
        // Зона заката: от altitude=+0.15 рад (день) до altitude=-0.15 рад (ночь)
        // t=0 → полный день, t=1 → полная ночь
        const float sunsetZone  = 0.15f; // ширина зоны перехода в радианах
        const float nightT = Clamp01((-altitude) / sunsetZone + 0.5f);
        const float dayT   = 1.0f - nightT;

        // --- SunLight CFrame ---
        // Ночью солнце "уходит" под горизонт — направляем свет снизу
        // Для этого если altitude < 0 берём зеркальное положение (отражение по Y)
        const float lightAlt  = (altitude >= 0.0f) ? altitude : -altitude;
        const float lightAz   = (altitude >= 0.0f) ? azimuth  : (azimuth + PI);
        const float lightSign = (altitude >= 0.0f) ? 1.0f     : -1.0f;

        MeturmRender::Types::CFrame sunCF = ToCFrame(
            Sunover::CFrame(
                Vector3(std::cosf(lightAlt) * std::sinf(lightAz) * 500.0f,
                        std::sinf(lightAlt) * 500.0f * lightSign,
                        std::cosf(lightAlt) * std::cosf(lightAz) * 500.0f),
                Matrix3x3::FromEuler(lightAlt * lightSign, lightAz, 0.0f)
            )
        );
        m_sunLight->SetCFrame(sunCF);

        // --- Интенсивность ---
        // Днём — нормальная яркость. Ночью — слабый лунный свет снизу (0.05)
        const float dayIntensity   = sunHeight01 * brightness;
        const float nightIntensity = 0.05f * brightness;
        m_sunLight->SetIntensity(dayIntensity * dayT + nightIntensity * nightT);

        // --- Цвет солнца (один расчёт, не меняется ночью) ---
        float sr, sg, sb;
        if (sunHeight01 < 0.08f)
        {
            float t = Clamp01(sunHeight01 / 0.08f);
            LerpColor(t, 1.0f, 0.2f, 0.05f, 1.0f, 0.6f, 0.3f, sr, sg, sb);
        }
        else if (sunHeight01 < 0.25f)
        {
            float t = Clamp01((sunHeight01 - 0.08f) / 0.17f);
            LerpColor(t, 1.0f, 0.6f, 0.3f, 1.0f, 0.95f, 0.85f, sr, sg, sb);
        }
        else
        {
            sr = 1.0f; sg = 0.98f; sb = 0.95f;
        }
        m_sunLight->SetColor(sr, sg, sb);

        // --- Ambient ---
        float ambientT = Clamp01(sunHeight01 * 4.0f);
        float ar, ag, ab;
        LerpColor(ambientT, 0.02f, 0.02f, 0.05f, 0.3f, 0.4f, 0.6f, ar, ag, ab);

        // Ночью ambient — тёмно-синий
        float nar = 0.02f, nag = 0.02f, nab = 0.06f;
        ar = ar * dayT + nar * nightT;
        ag = ag * dayT + nag * nightT;
        ab = ab * dayT + nab * nightT;

        m_sunLight->SetAmbient(ar * brightness, ag * brightness, ab * brightness);
        m_sunLight->SetOutdoorAmbient(ar * brightness * 1.2f,
                                      ag * brightness * 1.2f,
                                      ab * brightness * 1.2f);

        // --- Скайбоксы: переход день/ночь ---
        // Семантика SkyBox::SetTransparency: 1.0 = непрозрачный, 0.0 = прозрачный
        if (m_skyBox)
        {
            // Дневной скайбокс: 0=непрозрачный днём, 1=прозрачный ночью
            m_skyBox->SetTransparency(nightT);

            // Тинт заката: только в узкой зоне у горизонта (altitude от 0 до 0.1 рад)
            // sunsetPeak = 1 когда солнце точно на горизонте, 0 в остальное время
            const float sunsetPeak = Clamp01(1.0f - std::fabsf(altitude) / 0.12f)
                                   * Clamp01(1.0f - nightT); // гаснет ночью
            float tr, tg, tb;
            LerpColor(sunsetPeak,
                1.0f, 1.0f,  1.0f,   // белый (день)
                1.0f, 0.75f, 0.45f,  // слегка оранжевый (закат)
                tr, tg, tb);
            m_skyBox->SetColor(MeturmRender::Types::Color(tr, tg, tb, 1.0f));
        }

        if (m_skyBoxNight)
        {
            // Ночной скайбокс: 1=прозрачный днём, 0=непрозрачный ночью
            m_skyBoxNight->SetTransparency(dayT);
            m_skyBoxNight->SetColor(MeturmRender::Types::Color(1.0f, 1.0f, 1.0f, 1.0f));
        }

        // --- Меш солнца (billboard-квад) ---
        if (m_sunMesh)
        {
            Vector3 camPos = { 0.0f, 0.0f, 0.0f };
            for (auto& ci : m_dataModel->GetChildren())
            {
                if (ci->GetClassId() != Classes::CurrentCamera::ClassId) continue;
                auto* cfProp = ci->GetProperty(Classes::CurrentCamera::CFrame);
                if (cfProp && cfProp->Type == PropertyType::CFrame)
                    camPos = cfProp->Value.AsCFrame.Position;
                break;
            }

            const float sunDirX = -(std::cosf(altitude) * std::sinf(azimuth));
            const float sunDirY =   std::sinf(altitude);
            const float sunDirZ = -(std::cosf(altitude) * std::cosf(azimuth));

            const float sunDist = 100.0f;
            const Vector3 sunMeshPos = {
                camPos.X + sunDirX * sunDist,
                camPos.Y + sunDirY * sunDist,
                camPos.Z + sunDirZ * sunDist
            };

            static bool s_logged = false;
            if (!s_logged)
            {
                s_logged = true;
                std::cout << "[SunMesh] altitude=" << altitude
                          << " azimuth=" << azimuth
                          << " sunDir=(" << sunDirX << "," << sunDirY << "," << sunDirZ << ")"
                          << " camPos=(" << camPos.X << "," << camPos.Y << "," << camPos.Z << ")"
                          << " meshPos=(" << sunMeshPos.X << "," << sunMeshPos.Y << "," << sunMeshPos.Z << ")"
                          << " decals=" << m_sunMesh->GetDecals().size()
                          << " transparency=" << m_sunMesh->GetTransparency()
                          << std::endl;
            }

            const float fwdX = -sunDirX;
            const float fwdY = -sunDirY;
            const float fwdZ = -sunDirZ;

            float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
            if (std::fabsf(sunDirY) > 0.99f) { upY = 0.0f; upZ = 1.0f; }

            float rX = upY * fwdZ - upZ * fwdY;
            float rY = upZ * fwdX - upX * fwdZ;
            float rZ = upX * fwdY - upY * fwdX;
            const float rLen = std::sqrtf(rX*rX + rY*rY + rZ*rZ) + 1e-6f;
            rX /= rLen; rY /= rLen; rZ /= rLen;

            const float uX = fwdY * rZ - fwdZ * rY;
            const float uY = fwdZ * rX - fwdX * rZ;
            const float uZ = fwdX * rY - fwdY * rX;

            const Matrix3x3 billboardRot(
                rX, uX, fwdX,
                rY, uY, fwdY,
                rZ, uZ, fwdZ
            );

            m_sunMesh->SetCFrame(ToCFrame(Sunover::CFrame(sunMeshPos, billboardRot)));
            m_sunMesh->SetScale(30.0f, 30.0f, 1.0f);
        }
    }

} // namespace Sunover
