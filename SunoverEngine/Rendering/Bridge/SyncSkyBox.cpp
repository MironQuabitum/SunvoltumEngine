#include "BridgeCommon.h"

#include <MeturmRender/Objects/SkyBox.h>
#include <MeturmRender/Texture/Texture.h>
#include <MeturmRender/Types/SkyboxFace.h>
#include <fstream>
#include <filesystem>

namespace Sunover {

    // Загружает одну грань скайбокса из .dds файла
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
        using Face = MeturmRender::Enum::SkyboxFace;

        // Путь к скайбоксу относительно .exe
        fs::path skyDir = fs::path("PlatformContent") / "textures" / "sky";

        m_skyBox = new MeturmRender::Objects::SkyBox();

        auto rt = MeturmRender::RenderType::OpenGL;

        m_skyBox->SetTextures(
            LoadFace(rt, skyDir / "sky512_ft.dds"),   // Front  +Z
            LoadFace(rt, skyDir / "sky512_bk.dds"),   // Back   -Z
            LoadFace(rt, skyDir / "sky512_lf.dds"),   // Left   -X
            LoadFace(rt, skyDir / "sky512_rt.dds"),   // Right  +X
            LoadFace(rt, skyDir / "sky512_up.dds"),   // Top    +Y
            LoadFace(rt, skyDir / "sky512_dn.dds")    // Bottom -Y
        );
    }

} // namespace Sunover
