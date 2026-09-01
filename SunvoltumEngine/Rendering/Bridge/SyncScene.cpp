#include "BridgeCommon.h"
#include "../../DataModel/InstanceClasses/Workspace.h"
#include "../../DataModel/InstanceClasses/Model.h"
#include "../../DataModel/InstanceClasses/Folder.h"
#include <MeturmRender/Decal/Decal.h>
#include <MeturmRender/TextureSurface/TextureSurface.h>
#include <fstream>
#include <iostream>

namespace Sunvoltum {

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

    // -----------------------------------------------------------------------
    // SyncDecals — загружаем/обновляем декали на меше.
    // -----------------------------------------------------------------------
    static void SyncDecals(
        Instance*                                                              inst,
        uintptr_t                                                              key,
        MeturmRender::Objects::MeshObject&                                     meshObj,
        std::unordered_map<uintptr_t, std::unique_ptr<MeturmRender::Texture>>& texCache,
        std::unordered_map<uintptr_t, std::string>&                            pathCache,
        std::unordered_set<uintptr_t>&                                         syncedParts)
    {
        bool decalsDirty = false;

        for (auto& child : inst->GetChildren())
        {
            if (child->GetClassId() != Classes::Decal::ClassId) continue;

            auto* texProp = child->GetProperty(Classes::Decal::Texture);
            if (!texProp || texProp->Type != PropertyType::String) continue;

            const std::string& path = texProp->StringValue;
            if (path.empty()) continue;

            uintptr_t decalKey = reinterpret_cast<uintptr_t>(child.get());

            auto pathIt = pathCache.find(decalKey);
            bool pathChanged = (pathIt == pathCache.end()) || (pathIt->second != path);

            if (pathChanged)
            {
                std::ifstream stream(path, std::ios::binary);
                if (!stream.is_open()) continue;

                auto tex = std::make_unique<MeturmRender::Texture>();
                tex->LoadTexture(MeturmRender::RenderType::OpenGL, stream);
                if (!tex->IsLoaded()) continue;

                texCache[decalKey]  = std::move(tex);
                pathCache[decalKey] = path;
                decalsDirty = true;
            }
        }

        bool firstTime = (syncedParts.find(key) == syncedParts.end());

        if (decalsDirty || firstTime)
        {
            if (decalsDirty)
                meshObj.ClearDecals();

            for (auto& child : inst->GetChildren())
            {
                if (child->GetClassId() != Classes::Decal::ClassId) continue;

                uintptr_t decalKey = reinterpret_cast<uintptr_t>(child.get());
                auto texIt = texCache.find(decalKey);

                if (texIt == texCache.end())
                {
                    auto* texProp = child->GetProperty(Classes::Decal::Texture);
                    if (!texProp || texProp->Type != PropertyType::String) continue;
                    const std::string& path = texProp->StringValue;
                    if (path.empty()) continue;

                    std::ifstream stream(path, std::ios::binary);
                    if (!stream.is_open()) continue;

                    auto tex = std::make_unique<MeturmRender::Texture>();
                    tex->LoadTexture(MeturmRender::RenderType::OpenGL, stream);
                    if (!tex->IsLoaded()) continue;

                    pathCache[decalKey] = path;
                    texCache[decalKey]  = std::move(tex);
                    texIt = texCache.find(decalKey);
                }

                if (texIt == texCache.end() || !texIt->second->IsLoaded()) continue;

                int32_t faceInt = 4;
                auto* faceProp = child->GetProperty(Classes::Decal::Face);
                if (faceProp && faceProp->Type == PropertyType::Int)
                    faceInt = faceProp->Value.AsInt;

                meshObj.AddDecal(MeturmRender::Decal(*texIt->second, ToDecalFace(faceInt)));
            }

            syncedParts.insert(key);
        }
    }

    // -----------------------------------------------------------------------
    // SyncTextureSurfaces — добавляются один раз при регистрации объекта.
    // -----------------------------------------------------------------------
    static void SyncTextureSurfaces(
        Instance*                                                              inst,
        uintptr_t                                                              key,
        MeturmRender::Objects::MeshObject&                                     meshObj,
        std::unordered_map<uintptr_t, std::unique_ptr<MeturmRender::Texture>>& texCache,
        std::unordered_set<uintptr_t>&                                         syncedParts)
    {
        if (syncedParts.count(key)) return;

        for (auto& child : inst->GetChildren())
        {
            if (child->GetClassId() != Classes::TextureSurface::ClassId) continue;

            auto* texProp = child->GetProperty(Classes::TextureSurface::Texture);
            if (!texProp || texProp->Type != PropertyType::String) continue;

            const std::string& path = texProp->StringValue;
            if (path.empty()) continue;

            uintptr_t surfKey = reinterpret_cast<uintptr_t>(child.get());
            if (!texCache.count(surfKey))
            {
                std::ifstream stream(path, std::ios::binary);
                if (!stream.is_open()) continue;

                auto tex = std::make_unique<MeturmRender::Texture>();
                tex->LoadTexture(MeturmRender::RenderType::OpenGL, stream);
                texCache[surfKey] = std::move(tex);
            }

            MeturmRender::Texture* tex = texCache[surfKey].get();
            if (!tex || !tex->IsLoaded()) continue;

            int32_t faceInt = 0;
            auto* faceProp = child->GetProperty(Classes::TextureSurface::Face);
            if (faceProp && faceProp->Type == PropertyType::Int)
                faceInt = faceProp->Value.AsInt;

            MeturmRender::TextureSurface surface(*tex, ToDecalFace(faceInt));

            auto* studsU = child->GetProperty(Classes::TextureSurface::StudsPerTileU);
            if (studsU && studsU->Type == PropertyType::Float)
                surface.SetStudsPerTileU(studsU->Value.AsFloat);

            auto* studsV = child->GetProperty(Classes::TextureSurface::StudsPerTileV);
            if (studsV && studsV->Type == PropertyType::Float)
                surface.SetStudsPerTileV(studsV->Value.AsFloat);

            meshObj.AddTextureSurface(surface);
        }

        syncedParts.insert(key);
    }

    // -----------------------------------------------------------------------
    // RegisterSceneObject — вызывается один раз при добавлении ShapePart
    // в Workspace (через ChildAdded) или при инициализации для уже
    // существующих объектов. Создаёт SceneEntry, меш и подписки.
    // -----------------------------------------------------------------------
    void RenderBridge::RegisterSceneObject(Instance* inst, uintptr_t key)
    {
        if (m_scene.count(key)) return; // уже зарегистрирован

        SceneEntry entry;

        // --- Форма ---
        Sunvoltum::Shape shape = Sunvoltum::Shape::Block;
        auto* shapeProp = inst->GetProperty(Classes::ShapePart::Shape);
        if (shapeProp && shapeProp->Type == PropertyType::Shape)
            shape = shapeProp->Value.AsShape;

        Sunvoltum::Mesh sunMesh;
        switch (shape)
        {
            case Sunvoltum::Shape::Ball:
                sunMesh = Sunvoltum::Shapes::MakeSphere(0.5f, 24, 24);
                break;
            case Sunvoltum::Shape::Cylinder:
                sunMesh = Sunvoltum::Shapes::MakeCylinder(0.5f, 1.0f, 24);
                break;
            default:
                sunMesh = Sunvoltum::Shapes::MakeBlock(1.0f, 1.0f, 1.0f);
                break;
        }

        // --- Цвет ---
        Color3 color = { 1.0f, 1.0f, 1.0f };
        auto* colorProp = inst->GetProperty(Classes::ShapePart::Color);
        if (colorProp && colorProp->Type == PropertyType::Color3)
            color = colorProp->Value.AsColor3;
        for (auto& v : sunMesh.Vertices)
            v.Color = color;

        entry.mesh = std::make_unique<MeturmRender::Objects::MeshObject>(
            ToRenderMesh(sunMesh));

        // --- Начальные значения в кэш ---
        auto* cfProp = inst->GetProperty(Classes::ShapePart::CFrame);
        if (cfProp && cfProp->Type == PropertyType::CFrame)
            entry.cachedCFrame = cfProp->Value.AsCFrame;

        auto* sizeProp = inst->GetProperty(Classes::ShapePart::Size);
        if (sizeProp && sizeProp->Type == PropertyType::Vector3)
            entry.cachedSize = sizeProp->Value.AsVector3;

        auto* transProp = inst->GetProperty(Classes::ShapePart::Transparency);
        if (transProp && transProp->Type == PropertyType::Float)
            entry.cachedTransparency = transProp->Value.AsFloat;

        // --- isDynamic ---
        auto* anchorProp = inst->GetProperty(Classes::ShapePart::Anchored);
        if (anchorProp && anchorProp->Type == PropertyType::Bool)
            entry.isDynamic = !anchorProp->Value.AsBool;

        entry.dirtyCFrame       = true;
        entry.dirtySize         = true;
        entry.dirtyTransparency = true;

        // --- Подписки ---
        auto& pm = PropertyManager::Get();

        entry.tokenCFrame = pm.Subscribe(inst, Classes::ShapePart::CFrame,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type == PropertyType::CFrame)
                    it->second.cachedCFrame = val.Value.AsCFrame;
                it->second.dirtyCFrame = true;
            });

        entry.tokenSize = pm.Subscribe(inst, Classes::ShapePart::Size,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type == PropertyType::Vector3)
                    it->second.cachedSize = val.Value.AsVector3;
                it->second.dirtySize = true;
            });

        entry.tokenTransparency = pm.Subscribe(inst, Classes::ShapePart::Transparency,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type == PropertyType::Float)
                    it->second.cachedTransparency = val.Value.AsFloat;
                it->second.dirtyTransparency = true;
            });

        entry.tokenAnchored = pm.Subscribe(inst, Classes::ShapePart::Anchored,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type == PropertyType::Bool)
                {
                    it->second.isDynamic = !val.Value.AsBool;
                    it->second.dirtyCFrame       = true;
                    it->second.dirtySize         = true;
                    it->second.dirtyTransparency = true;
                }
            });

        m_sceneOrder.push_back(key);
        m_scene[key] = std::move(entry);
    }

    // -----------------------------------------------------------------------
    // RegisterInstanceRecursive — рекурсивно регистрирует ShapePart'ы.
    // Если inst — ShapePart, регистрирует его напрямую.
    // Если inst — контейнер (Model, Folder, Workspace, etc.) — обходит детей.
    // Также подписывается на ChildAdded контейнера для будущих добавлений.
    // -----------------------------------------------------------------------
    void RenderBridge::RegisterInstanceRecursive(Instance* inst)
    {
        if (!inst) return;

        const int8_t cls = inst->GetClassId();

        if (cls == Classes::ShapePart::ClassId)
        {
            uintptr_t key = reinterpret_cast<uintptr_t>(inst);
            RegisterSceneObject(inst, key);
            return;
        }

        // Для контейнеров: обходим детей и подписываемся на ChildAdded
        bool isContainer = (cls == Classes::CLASS_MODEL)
                        || (cls == Classes::CLASS_FOLDER)
                        || (cls == Classes::Workspace::ClassId);

        if (!isContainer) return;

        // Регистрируем уже существующих детей
        for (auto& child : inst->GetChildren())
            RegisterInstanceRecursive(child.get());

        // Подписываемся на будущих детей — храним токен в m_containerTokens
        m_containerTokens.push_back(inst->SubscribeChildAdded(
            [this](Instance& child)
            {
                RegisterInstanceRecursive(&child);
            }));
    }

    // -----------------------------------------------------------------------
    // SubscribeWorkspace — регистрирует ShapePart'ы в Workspace (и вложенных
    // контейнерах — Model, Folder) рекурсивно.
    // -----------------------------------------------------------------------
    void RenderBridge::SubscribeWorkspace()
    {
        Instance* ws = m_dataModel->FindByName("Workspace");
        if (!ws)
        {
            std::cerr << "[RenderBridge] SubscribeWorkspace: Workspace not found\n";
            return;
        }

        // Регистрируем всё рекурсивно (Workspace сам является контейнером)
        RegisterInstanceRecursive(ws);
    }

    // -----------------------------------------------------------------------
    // SyncScene — рендер всей сцены.
    //
    // Итерирует только m_sceneOrder — список ключей ShapePart'ов зарегистрированных
    // через SubscribeWorkspace. Никакого поиска новых объектов здесь нет.
    //
    // Dynamic (Anchored=false): CFrame каждый кадр из кэша.
    // Static  (Anchored=true):  только по dirty-флагу.
    // -----------------------------------------------------------------------
    void RenderBridge::SyncScene()
    {
        for (uintptr_t key : m_sceneOrder)
        {
            auto it = m_scene.find(key);
            if (it == m_scene.end()) continue;

            SceneEntry& entry = it->second;
            Instance*   inst  = reinterpret_cast<Instance*>(key);

            // --- CFrame ---
            if (entry.isDynamic)
                entry.mesh->SetCFrame(ToCFrame(entry.cachedCFrame));
            else if (entry.dirtyCFrame)
            {
                entry.mesh->SetCFrame(ToCFrame(entry.cachedCFrame));
                entry.dirtyCFrame = false;
            }

            // --- Size ---
            if (entry.dirtySize)
            {
                entry.mesh->SetScale(entry.cachedSize.X,
                                     entry.cachedSize.Y,
                                     entry.cachedSize.Z);
                entry.dirtySize = false;
            }

            // --- Transparency ---
            if (entry.dirtyTransparency)
            {
                entry.mesh->SetTransparency(entry.cachedTransparency);
                entry.dirtyTransparency = false;
            }

            // --- Декали и поверхности ---
            SyncDecals(inst, key, *entry.mesh,
                       m_decalTextureCache, m_decalPathCache, m_decalSyncedParts);

            SyncTextureSurfaces(inst, key, *entry.mesh,
                                m_surfaceTextureCache, m_surfaceSyncedParts);

            // --- Рендер ---
            m_renderer->RenderObject(*entry.mesh, *m_camera);
        }
    }

} // namespace Sunvoltum
