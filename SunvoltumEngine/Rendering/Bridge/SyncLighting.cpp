#include "BridgeCommon.h"
#include "../../DataModel/InstanceClasses/CurrentCamera.h"
#include <MeturmRender/Objects/SkyBox.h>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Sunvoltum {

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

    // -------------------------------------------------------------------------
    // SubscribeLighting — вызывается один раз из Init.
    // Находит Lighting Instance, кэширует начальные значения,
    // подписывается на ClockTime / GeographicLatitude / Brightness.
    // -------------------------------------------------------------------------
    void RenderBridge::SubscribeLighting()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::Lighting::ClassId) continue;
            m_lightingInst = inst.get();
            break;
        }
        if (!m_lightingInst) return;

        // Считываем начальные значения
        auto* ct = m_lightingInst->GetProperty(Classes::Lighting::ClockTime);
        if (ct && ct->Type == PropertyType::Number)
            m_clockTime = static_cast<float>(ct->Value.AsNumber);

        auto* lat = m_lightingInst->GetProperty(Classes::Lighting::GeographicLatitude);
        if (lat && lat->Type == PropertyType::Number)
            m_latitude = static_cast<float>(lat->Value.AsNumber);

        auto* br = m_lightingInst->GetProperty(Classes::Lighting::Brightness);
        if (br && br->Type == PropertyType::Number)
            m_brightness = static_cast<float>(br->Value.AsNumber);

        auto& pm = PropertyManager::Get();

        // ClockTime меняется каждый кадр в demo (clocktime += dt*0.1f) —
        // подписка гарантирует пересчёт только когда значение реально изменилось,
        // а не каждый кадр вызывать SyncLighting вхолостую.
        m_lightClockToken = pm.Subscribe(m_lightingInst, Classes::Lighting::ClockTime,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::Number)
                    m_clockTime = static_cast<float>(val.Value.AsNumber);
                m_needLightingSync = true;
            });

        m_lightLatToken = pm.Subscribe(m_lightingInst, Classes::Lighting::GeographicLatitude,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::Number)
                    m_latitude = static_cast<float>(val.Value.AsNumber);
                m_needLightingSync = true;
            });

        m_lightBrightToken = pm.Subscribe(m_lightingInst, Classes::Lighting::Brightness,
            [this](const PropertyValue& val)
            {
                if (val.Type == PropertyType::Number)
                    m_brightness = static_cast<float>(val.Value.AsNumber);
                m_needLightingSync = true;
            });

        // Первый расчёт
        m_needLightingSync = true;
    }

    // -------------------------------------------------------------------------
    // RecalcLighting — вычисляет и применяет освещение из закэшированных значений.
    // Вызывается из SyncLighting() только если m_needLightingSync == true.
    // Позиция камеры для billboard sun/moon читается каждый раз (меняется каждый кадр).
    // -------------------------------------------------------------------------
    void RenderBridge::RecalcLighting()
    {
        const float clockTime  = m_clockTime;
        const float latitude   = m_latitude;
        const float brightness = m_brightness;

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

        const float sunHeight01 = Clamp01(altitude / (PI * 0.5f));

        // --- Переход день/ночь ---
        const float sunsetZone = 0.15f;
        const float nightT = Clamp01((-altitude) / sunsetZone + 0.5f);
        const float dayT   = 1.0f - nightT;

        const float moonAlt = -altitude;
        const float moonAz  = azimuth + PI;

        const float lightAlt = altitude * dayT + moonAlt * nightT;
        const float lightAz  = azimuth  * dayT + moonAz  * nightT;

        MeturmRender::Types::CFrame lightCF = ToCFrame(
            Sunvoltum::CFrame(
                Vector3(std::cosf(lightAlt) * std::sinf(lightAz) * 500.0f,
                        std::sinf(lightAlt) * 500.0f,
                        std::cosf(lightAlt) * std::cosf(lightAz) * 500.0f),
                Matrix3x3::FromEuler(lightAlt, lightAz, 0.0f)
            )
        );
        m_sunLight->SetCFrame(lightCF);

        const float dayIntensity  = sunHeight01 * brightness;
        const float moonIntensity = 0.4f * brightness;
        m_sunLight->SetIntensity(dayIntensity * dayT + moonIntensity * nightT);

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
        const float lr = 1.0f, lg = 1.0f, lb = 1.0f;
        m_sunLight->SetColor(sr * dayT + lr * nightT,
                             sg * dayT + lg * nightT,
                             sb * dayT + lb * nightT);

        float ambientT = Clamp01(sunHeight01 * 4.0f);
        float ar, ag, ab;
        LerpColor(ambientT, 0.02f, 0.02f, 0.05f, 0.3f, 0.4f, 0.6f, ar, ag, ab);
        float nar = 0.02f, nag = 0.02f, nab = 0.06f;
        ar = ar * dayT + nar * nightT;
        ag = ag * dayT + nag * nightT;
        ab = ab * dayT + nab * nightT;
        m_sunLight->SetAmbient(ar * brightness, ag * brightness, ab * brightness);
        m_sunLight->SetOutdoorAmbient(ar * brightness * 1.2f,
                                      ag * brightness * 1.2f,
                                      ab * brightness * 1.2f);

        // --- Скайбоксы ---
        if (m_skyBox)
        {
            m_skyBox->SetTransparency(nightT);
            const float sunsetPeak = Clamp01(1.0f - std::fabsf(altitude) / 0.12f)
                                   * Clamp01(1.0f - nightT);
            float tr, tg, tb;
            LerpColor(sunsetPeak, 1.0f, 1.0f, 1.0f, 1.0f, 0.75f, 0.45f, tr, tg, tb);
            m_skyBox->SetColor(MeturmRender::Types::Color(tr, tg, tb, 1.0f));
        }
        if (m_skyBoxNight)
        {
            m_skyBoxNight->SetTransparency(dayT);
            m_skyBoxNight->SetColor(MeturmRender::Types::Color(1.0f, 1.0f, 1.0f, 1.0f));
        }

        // --- Билборды солнца и луны ---
        // Позиция камеры читается каждый раз — она меняется каждый кадр.
        Vector3 camPos = { 0.0f, 0.0f, 0.0f };
        if (m_cameraInst)
        {
            const auto* cfProp = m_cameraInst->GetProperty(Classes::CurrentCamera::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
                camPos = cfProp->Value.AsCFrame.Position;
        }

        auto MakeBillboard = [&](float dirAlt, float dirAz, float dist,
                                 MeturmRender::Objects::MeshObject* mesh,
                                 float scaleXY)
        {
            if (!mesh) return;
            const float dx = -(std::cosf(dirAlt) * std::sinf(dirAz));
            const float dy =   std::sinf(dirAlt);
            const float dz = -(std::cosf(dirAlt) * std::cosf(dirAz));

            const Vector3 pos = { camPos.X + dx * dist,
                                  camPos.Y + dy * dist,
                                  camPos.Z + dz * dist };

            const float fwdX = -dx, fwdY = -dy, fwdZ = -dz;
            float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
            if (std::fabsf(dy) > 0.99f) { upY = 0.0f; upZ = 1.0f; }

            float rX = upY * fwdZ - upZ * fwdY;
            float rY = upZ * fwdX - upX * fwdZ;
            float rZ = upX * fwdY - upY * fwdX;
            const float rLen = std::sqrtf(rX*rX + rY*rY + rZ*rZ) + 1e-6f;
            rX /= rLen; rY /= rLen; rZ /= rLen;

            const float uX = fwdY * rZ - fwdZ * rY;
            const float uY = fwdZ * rX - fwdX * rZ;
            const float uZ = fwdX * rY - fwdY * rX;

            const Matrix3x3 rot(rX, uX, fwdX, rY, uY, fwdY, rZ, uZ, fwdZ);
            mesh->SetCFrame(ToCFrame(Sunvoltum::CFrame(pos, rot)));
            mesh->SetScale(scaleXY, scaleXY, 1.0f);
        };

        MakeBillboard(altitude, azimuth, 100.0f, m_sunMesh.get(),  30.0f);
        MakeBillboard(moonAlt,  moonAz,  100.0f, m_moonMesh.get(), 25.0f);
    }

    // -------------------------------------------------------------------------
    // SyncLighting — вызывается каждый кадр из Frame().
    // Пересчёт происходит только если m_needLightingSync == true.
    // ClockTime меняется каждый кадр в demo → флаг ставится каждый кадр,
    // но сам расчёт вынесен в RecalcLighting() и не дублируется.
    // -------------------------------------------------------------------------
    void RenderBridge::SyncLighting()
    {
        if (!m_needLightingSync) return;
        m_needLightingSync = false;
        RecalcLighting();
    }

} // namespace Sunvoltum
