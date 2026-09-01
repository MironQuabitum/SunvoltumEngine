#include "BridgeCommon.h"

#include <MeturmRender/Objects/SkyBox.h>
#include <MeturmRender/Texture/Texture.h>
#include <MeturmRender/Types/SkyboxFace.h>
#include <fstream>
#include <filesystem>
#include <iostream>
namespace Sunvoltum {

    // Загружает одну грань скайбокса из файла
    static MeturmRender::Texture LoadFace(MeturmRender::RenderType renderType,
                                          const std::filesystem::path& path)
    {
        MeturmRender::Texture tex;
        std::ifstream file(path, std::ios::binary);
        if (file.is_open())
            tex.LoadTexture(renderType, file);
        return tex;
    }

    void RenderBridge::InitSkyBox()
    {
        namespace fs = std::filesystem;

        fs::path skyDir = fs::path("PlatformContent") / "textures" / "sky";
        auto rt = MeturmRender::RenderType::OpenGL;

        // --- Дневной скайбокс (6 граней DDS) ---
        m_skyBox = new MeturmRender::Objects::SkyBox();
        m_skyBox->SetTextures(
            LoadFace(rt, skyDir / "sky512_ft.dds"),   // Front  +Z
            LoadFace(rt, skyDir / "sky512_bk.dds"),   // Back   -Z
            LoadFace(rt, skyDir / "sky512_lf.dds"),   // Left   -X
            LoadFace(rt, skyDir / "sky512_rt.dds"),   // Right  +X
            LoadFace(rt, skyDir / "sky512_up.dds"),   // Top    +Y
            LoadFace(rt, skyDir / "sky512_dn.dds")    // Bottom -Y
        );
        m_skyBox->SetTransparency(0.0f); // начинаем непрозрачным (0=непрозрачный)

        // --- Ночной скайбокс ---
        m_skyBoxNight = new MeturmRender::Objects::SkyBox();
        {
            MeturmRender::Texture nightTex = LoadFace(rt, skyDir / "sky512night.dds");

            if (!nightTex.IsLoaded())
            {
                // Файл не найден — генерируем процедурное ночное небо (тёмно-синий градиент)
                std::cout << "[SkyBox] sky512night.dds not found, using procedural night sky" << std::endl;

                // 4x4 RGBA: тёмно-синий
                std::vector<uint8_t> px(4 * 4 * 4);
                for (int i = 0; i < 4 * 4; ++i)
                {
                    px[i*4 + 0] = 5;   // R
                    px[i*4 + 1] = 5;   // G
                    px[i*4 + 2] = 20;  // B
                    px[i*4 + 3] = 255; // A
                }
                nightTex.LoadRawRGBA(rt, 4, 4, px);
            }
            else
            {
                std::cout << "[SkyBox] sky512night.dds loaded OK" << std::endl;
            }

            m_skyBoxNight->SetTextures(
                nightTex, nightTex, nightTex,
                nightTex, nightTex, nightTex
            );
        }
        m_skyBoxNight->SetTransparency(1.0f); // начинаем полностью прозрачным (невидим)
    }

} // namespace Sunvoltum
