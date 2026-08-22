#include "BridgeCommon.h"
#include <MeturmRender/Decal/Decal.h>
#include <MeturmRender/TextureSurface/TextureSurface.h>
#include <fstream>

namespace Sunover {

    // Конвертирует целочисленное значение свойства Face в MeturmRender::Enum::DecalFace.
    // Порядок соответствует Decal.h: 0=Top, 1=Bottom, 2=Left, 3=Right, 4=Front, 5=Back
    static MeturmRender::Enum::DecalFace ToDecalFace(int32_t faceInt)
    {
        switch (faceInt)
        {
            case 0:  return MeturmRender::Enum::DecalFace::Top;
            case 1:  return MeturmRender::Enum::DecalFace::Bottom;
            case 2:  return MeturmRender::Enum::DecalFace::Left;
            case 3:  return MeturmRender::Enum::DecalFace::Right;
            case 5:  return MeturmRender::Enum::DecalFace::Back;
            case 4:
            default: return MeturmRender::Enum::DecalFace::Front;
        }
    }

    void RenderBridge::SyncScene()
    {
        for (auto& inst : m_dataModel->GetChildren())
        {
            if (inst->GetClassId() != Classes::ShapePart::ClassId) continue;

            uintptr_t key = reinterpret_cast<uintptr_t>(inst.get());

            // --- Создаём MeshObject только если его ещё нет в кэше ---
            // Объект живёт в m_meshCache всё время работы движка, не пересоздаётся каждый кадр.
            // Это устраняет crash: ANGLE обращается к GPU-буферам внутри EndFrame() —
            // к этому моменту объект должен быть жив.
            if (m_meshCache.find(key) == m_meshCache.end())
            {
                // --- Форма ---
                Sunover::Shape shape = Sunover::Shape::Block;
                auto* shapeProp = inst->GetProperty(Classes::ShapePart::Shape);
                if (shapeProp && shapeProp->Type == PropertyType::Shape)
                    shape = shapeProp->Value.AsShape;

                // Генерируем меш нужной формы
                Sunover::Mesh sunoverMesh;
                switch (shape)
                {
                    case Sunover::Shape::Ball:
                        sunoverMesh = Sunover::Shapes::MakeSphere(0.5f, 24, 24);
                        break;
                    case Sunover::Shape::Cylinder:
                        sunoverMesh = Sunover::Shapes::MakeCylinder(0.5f, 1.0f, 24);
                        break;
                    case Sunover::Shape::Block:
                    default:
                        sunoverMesh = Sunover::Shapes::MakeBlock(1.0f, 1.0f, 1.0f);
                        break;
                }

                // --- Цвет --- красим вершины меша
                Color3 color = { 1.0f, 1.0f, 1.0f };
                auto* colorProp = inst->GetProperty(Classes::ShapePart::Color);
                if (colorProp && colorProp->Type == PropertyType::Color3)
                    color = colorProp->Value.AsColor3;

                for (auto& v : sunoverMesh.Vertices)
                    v.Color = color;

                // --- Конвертируем меш в формат MeturmRender и кладём в кэш ---
                // MeshObject не копируется (copy ctor = delete), передаём mesh через move.
                auto renderMesh = ToRenderMesh(sunoverMesh);
                m_meshCache[key] = std::make_unique<MeturmRender::Objects::MeshObject>(
                    std::move(renderMesh));
            }

            MeturmRender::Objects::MeshObject& meshObj = *m_meshCache[key];

            // --- CFrame обновляется каждый кадр (только constant buffer, без реаллокации) ---
            auto* cfProp = inst->GetProperty(Classes::ShapePart::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
                meshObj.SetCFrame(ToCFrame(cfProp->Value.AsCFrame));

            // --- Size — масштаб объекта по трём осям ---
            auto* sizeProp = inst->GetProperty(Classes::ShapePart::Size);
            if (sizeProp && sizeProp->Type == PropertyType::Vector3)
            {
                const auto& s = sizeProp->Value.AsVector3;
                meshObj.SetScale(s.X, s.Y, s.Z);
            }

            // --- Прозрачность ---
            auto* transProp = inst->GetProperty(Classes::ShapePart::Transparency);
            if (transProp && transProp->Type == PropertyType::Float)
                meshObj.SetTransparency(transProp->Value.AsFloat);

            // --- Декали ---
            // Добавляются один раз при первом обнаружении Part'а.
            // Пересоздание не нужно: декаль хранится внутри MeshObject persistent.
            if (m_decalSyncedParts.find(key) == m_decalSyncedParts.end())
            {
                for (auto& child : inst->GetChildren())
                {
                    if (child->GetClassId() != Classes::Decal::ClassId) continue;

                    auto* texProp = child->GetProperty(Classes::Decal::Texture);
                    if (!texProp || texProp->Type != PropertyType::String) continue;

                    const std::string& texturePath = texProp->StringValue;
                    if (texturePath.empty()) continue;

                    uintptr_t decalKey = reinterpret_cast<uintptr_t>(child.get());
                    if (m_decalTextureCache.find(decalKey) == m_decalTextureCache.end())
                    {
                        std::ifstream stream(texturePath, std::ios::binary);
                        if (!stream.is_open()) continue;

                        auto tex = std::make_unique<MeturmRender::Texture>();
                        tex->LoadTexture(MeturmRender::RenderType::OpenGL, stream);
                        m_decalTextureCache[decalKey] = std::move(tex);
                    }

                    MeturmRender::Texture* tex = m_decalTextureCache[decalKey].get();
                    if (!tex || !tex->IsLoaded()) continue;

                    int32_t faceInt = 4; // Front по умолчанию
                    auto* faceProp = child->GetProperty(Classes::Decal::Face);
                    if (faceProp && faceProp->Type == PropertyType::Int)
                        faceInt = faceProp->Value.AsInt;

                    MeturmRender::Decal decal(*tex, ToDecalFace(faceInt));
                    meshObj.AddDecal(decal);
                }
                m_decalSyncedParts.insert(key);
            }

            // --- TextureSurface ---
            // Аналогично декалям — добавляются один раз.
            if (m_surfaceSyncedParts.find(key) == m_surfaceSyncedParts.end())
            {
                for (auto& child : inst->GetChildren())
                {
                    if (child->GetClassId() != Classes::TextureSurface::ClassId) continue;

                    auto* texProp = child->GetProperty(Classes::TextureSurface::Texture);
                    if (!texProp || texProp->Type != PropertyType::String) continue;

                    const std::string& texturePath = texProp->StringValue;
                    if (texturePath.empty()) continue;

                    uintptr_t surfKey = reinterpret_cast<uintptr_t>(child.get());
                    if (m_surfaceTextureCache.find(surfKey) == m_surfaceTextureCache.end())
                    {
                        std::ifstream stream(texturePath, std::ios::binary);
                        if (!stream.is_open()) continue;

                        auto tex = std::make_unique<MeturmRender::Texture>();
                        tex->LoadTexture(MeturmRender::RenderType::OpenGL, stream);
                        m_surfaceTextureCache[surfKey] = std::move(tex);
                    }

                    MeturmRender::Texture* tex = m_surfaceTextureCache[surfKey].get();
                    if (!tex || !tex->IsLoaded()) continue;

                    int32_t faceInt = 0; // Top по умолчанию для поверхности
                    auto* faceProp = child->GetProperty(Classes::TextureSurface::Face);
                    if (faceProp && faceProp->Type == PropertyType::Int)
                        faceInt = faceProp->Value.AsInt;

                    MeturmRender::TextureSurface surface(*tex, ToDecalFace(faceInt));

                    auto* studsUProp = child->GetProperty(Classes::TextureSurface::StudsPerTileU);
                    if (studsUProp && studsUProp->Type == PropertyType::Float)
                        surface.SetStudsPerTileU(studsUProp->Value.AsFloat);

                    auto* studsVProp = child->GetProperty(Classes::TextureSurface::StudsPerTileV);
                    if (studsVProp && studsVProp->Type == PropertyType::Float)
                        surface.SetStudsPerTileV(studsVProp->Value.AsFloat);

                    meshObj.AddTextureSurface(surface);
                }
                m_surfaceSyncedParts.insert(key);
            }

            m_renderer->RenderObject(meshObj, *m_camera);
        }
    }

} // namespace Sunover
