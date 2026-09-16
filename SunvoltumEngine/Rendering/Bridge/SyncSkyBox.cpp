#include "BridgeCommon.h"

#include <SunvoltumRender/Objects/SkyBox.h>
#include <SunvoltumRender/Core/Texture.h>
#include <SunvoltumRender/Types/SkyboxFace.h>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace Sunvoltum {

    // Загружает одну грань скайбокса из файла
    static SunvoltumRender::Texture LoadFace(const std::filesystem::path& path)
    {
        SunvoltumRender::Texture tex;
        std::ifstream file(path, std::ios::binary);
        if (file.is_open())
            tex.LoadStream(file);
        return tex;
    }

    void RenderBridge::InitSkyBox()
    {
        namespace fs = std::filesystem;

        fs::path skyDir = fs::path("PlatformContent") / "textures" / "sky";

        // --- Дневной скайбокс (6 граней DDS) ---
        m_skyBox = std::make_unique<SunvoltumRender::Objects::SkyBox>();
        m_skyBox->SetTextures(
            LoadFace(skyDir / "sky512_ft.dds"),   // Front  +Z
            LoadFace(skyDir / "sky512_bk.dds"),   // Back   -Z
            LoadFace(skyDir / "sky512_lf.dds"),   // Left   -X
            LoadFace(skyDir / "sky512_rt.dds"),   // Right  +X
            LoadFace(skyDir / "sky512_up.dds"),   // Top    +Y
            LoadFace(skyDir / "sky512_dn.dds")    // Bottom -Y
        );
        m_skyBox->SetTransparency(0.0f); // начинаем непрозрачным (0=непрозрачный)

        // --- Ночной скайбокс ---
        m_skyBoxNight = std::make_unique<SunvoltumRender::Objects::SkyBox>();
        {
            SunvoltumRender::Texture nightTex = LoadFace(skyDir / "sky512night.dds");

            if (!nightTex.IsLoaded())
            {
                // Файл не найден — генерируем процедурное ночное небо (тёмно-синий)
                // 4x4 RGBA: тёмно-синий
                std::vector<uint8_t> px(4 * 4 * 4);
                for (int i = 0; i < 4 * 4; ++i)
                {
                    px[i*4 + 0] = 5;   // R
                    px[i*4 + 1] = 5;   // G
                    px[i*4 + 2] = 20;  // B
                    px[i*4 + 3] = 255; // A
                }
                nightTex.LoadRawRGBA(4, 4, px);
            }

            m_skyBoxNight->SetTextures(
                nightTex, nightTex, nightTex,
                nightTex, nightTex, nightTex
            );
        }
        m_skyBoxNight->SetTransparency(1.0f); // начинаем полностью прозрачным (невидим)
    }

} // namespace Sunvoltum
