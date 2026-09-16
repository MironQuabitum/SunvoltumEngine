#include "BridgeCommon.h"
#include "../../DataModel/InstanceClasses/BasePart.h"
#include "../../DataModel/InstanceClasses/ShapePart.h"
#include "../../DataModel/InstanceClasses/Workspace.h"
#include "../../DataModel/InstanceClasses/Model.h"
#include "../../DataModel/InstanceClasses/Folder.h"
#include "../../DataModel/InstanceClasses/Decal.h"
#include "../../DataModel/InstanceClasses/TextureSurface.h"
#include "../../DataModel/InstanceRegistry.h"
#include <SunvoltumRender/Objects/MeshObject.h>
#include <SunvoltumRender/Objects/Textures/Decal.h>
#include <SunvoltumRender/Objects/Textures/TextureSurface.h>
#include <SunvoltumRender/Types/DecalFace.h>
#include <SunvoltumRender/Renderer/Renderer.h>
#include <fstream>
#include <iostream>
#include <cmath>

namespace Sunvoltum {

    // -----------------------------------------------------------------------
    // BuildMesh — строит меш по форме и красит вершины в заданный цвет.
    // Выносим логику сюда, чтобы вызывать и при регистрации, и при смене
    // Shape/Color через подписки.
    // -----------------------------------------------------------------------
    static std::unique_ptr<SunvoltumRender::Objects::MeshObject>
    BuildMesh(Sunvoltum::Shape shape, const Color3& color)
    {
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
        for (auto& v : sunMesh.Vertices)
            v.Color = color;
        return std::make_unique<SunvoltumRender::Objects::MeshObject>(ToRenderMesh(sunMesh));
    }

    // -----------------------------------------------------------------------
    // LerpCFrame — линейная интерполяция позиции + nlerp матрицы вращения.
    // Нормализация через деление на норму строк — достаточна для плавного
    // визуального результата без полноценного slerp через кватернионы.
    // -----------------------------------------------------------------------
    static CFrame LerpCFrame(const CFrame& a, const CFrame& b, float t)
    {
        // Позиция — обычный lerp
        Vector3 pos(
            a.Position.X + (b.Position.X - a.Position.X) * t,
            a.Position.Y + (b.Position.Y - a.Position.Y) * t,
            a.Position.Z + (b.Position.Z - a.Position.Z) * t
        );

        // Вращение — покомпонентный lerp с последующей ортонормализацией (Gram-Schmidt)
        float r00 = a.Rotation.R00 + (b.Rotation.R00 - a.Rotation.R00) * t;
        float r01 = a.Rotation.R01 + (b.Rotation.R01 - a.Rotation.R01) * t;
        float r02 = a.Rotation.R02 + (b.Rotation.R02 - a.Rotation.R02) * t;
        float r10 = a.Rotation.R10 + (b.Rotation.R10 - a.Rotation.R10) * t;
        float r11 = a.Rotation.R11 + (b.Rotation.R11 - a.Rotation.R11) * t;
        float r12 = a.Rotation.R12 + (b.Rotation.R12 - a.Rotation.R12) * t;
        float r20 = a.Rotation.R20 + (b.Rotation.R20 - a.Rotation.R20) * t;
        float r21 = a.Rotation.R21 + (b.Rotation.R21 - a.Rotation.R21) * t;
        float r22 = a.Rotation.R22 + (b.Rotation.R22 - a.Rotation.R22) * t;

        // Нормализуем первую строку
        float n0 = std::sqrt(r00*r00 + r01*r01 + r02*r02);
        if (n0 > 1e-6f) { r00 /= n0; r01 /= n0; r02 /= n0; }

        // Вторая строка — убираем проекцию на первую (Gram-Schmidt), нормализуем
        float dot01 = r10*r00 + r11*r01 + r12*r02;
        r10 -= dot01 * r00; r11 -= dot01 * r01; r12 -= dot01 * r02;
        float n1 = std::sqrt(r10*r10 + r11*r11 + r12*r12);
        if (n1 > 1e-6f) { r10 /= n1; r11 /= n1; r12 /= n1; }

        // Третья строка — кросс-произведение первых двух (гарантирует правостороннюю СК)
        r20 = r01*r12 - r02*r11;
        r21 = r02*r10 - r00*r12;
        r22 = r00*r11 - r01*r10;

        return CFrame(pos, Matrix3x3(r00, r01, r02,
                                     r10, r11, r12,
                                     r20, r21, r22));
    }

    static SunvoltumRender::Enum::DecalFace ToDecalFace(int32_t faceInt)
    {
        switch (faceInt)
        {
            case 0:  return SunvoltumRender::Enum::DecalFace::Top;
            case 1:  return SunvoltumRender::Enum::DecalFace::Bottom;
            case 2:  return SunvoltumRender::Enum::DecalFace::Left;
            case 3:  return SunvoltumRender::Enum::DecalFace::Right;
            case 5:  return SunvoltumRender::Enum::DecalFace::Back;
            case 4:
            default: return SunvoltumRender::Enum::DecalFace::Front;
        }
    }

    // -----------------------------------------------------------------------
    // SyncDecals — загружаем/обновляем декали на меше.
    // -----------------------------------------------------------------------
    static void SyncDecals(
        Instance*                                                              inst,
        uintptr_t                                                              key,
        SunvoltumRender::Objects::MeshObject&                                  meshObj,
        std::unordered_map<uintptr_t, std::unique_ptr<SunvoltumRender::Texture>>& texCache,
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

                auto tex = std::make_unique<SunvoltumRender::Texture>();
                tex->LoadStream(stream);
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

                    auto tex = std::make_unique<SunvoltumRender::Texture>();
                    tex->LoadStream(stream);
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

                meshObj.AddDecal(SunvoltumRender::Objects::Decal(*texIt->second, ToDecalFace(faceInt)));
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
        SunvoltumRender::Objects::MeshObject&                                  meshObj,
        std::unordered_map<uintptr_t, std::unique_ptr<SunvoltumRender::Texture>>& texCache,
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

                auto tex = std::make_unique<SunvoltumRender::Texture>();
                tex->LoadStream(stream);
                texCache[surfKey] = std::move(tex);
            }

            SunvoltumRender::Texture* tex = texCache[surfKey].get();
            if (!tex || !tex->IsLoaded()) continue;

            int32_t faceInt = 0;
            auto* faceProp = child->GetProperty(Classes::TextureSurface::Face);
            if (faceProp && faceProp->Type == PropertyType::Int)
                faceInt = faceProp->Value.AsInt;

            SunvoltumRender::Objects::TextureSurface surface(*tex, ToDecalFace(faceInt));

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
        entry.cachedShape = Sunvoltum::Shape::Block;
        auto* shapeProp = inst->GetProperty(Classes::BasePart::Shape);
        if (shapeProp && shapeProp->Type == PropertyType::Shape)
            entry.cachedShape = shapeProp->Value.AsShape;

        // --- Цвет ---
        entry.cachedColor = { 1.0f, 1.0f, 1.0f };
        auto* colorProp = inst->GetProperty(Classes::BasePart::Color);
        if (colorProp && colorProp->Type == PropertyType::Color3)
            entry.cachedColor = colorProp->Value.AsColor3;

        // Строим меш один раз с нужной формой и цветом
        entry.mesh = BuildMesh(entry.cachedShape, entry.cachedColor);

        // --- Начальные значения в кэш ---
        auto* cfProp = inst->GetProperty(Classes::BasePart::CFrame);
        if (cfProp && cfProp->Type == PropertyType::CFrame)
            entry.cachedCFrame = cfProp->Value.AsCFrame;

        auto* sizeProp = inst->GetProperty(Classes::BasePart::Size);
        if (sizeProp && sizeProp->Type == PropertyType::Vector3)
            entry.cachedSize = sizeProp->Value.AsVector3;

        auto* transProp = inst->GetProperty(Classes::BasePart::Transparency);
        if (transProp && transProp->Type == PropertyType::Float)
            entry.cachedTransparency = transProp->Value.AsFloat;

        // --- isDynamic ---
        auto* anchorProp = inst->GetProperty(Classes::BasePart::Anchored);
        if (anchorProp && anchorProp->Type == PropertyType::Bool)
            entry.isDynamic = !anchorProp->Value.AsBool;

        // --- isNetworkControlled ---
        // Объект network-controlled если он динамический (Anchored=false) и не
        // принадлежит локальной физике — т.е. его позиция приходит по сети.
        // На рендер-клиенте это все dynamic ShapePart'ы: owned-объект управляется
        // локальной физикой (isDynamic), но интерполяция для него не нужна —
        // его позиция и так обновляется каждый физический тик в cachedCFrame.
        // Флаг можно уточнить позже через SetNetworkControlled(bool) от клиента,
        // пока ставим true для всех dynamic — интерполяция не навредит owned-объекту
        // если interpPeriod мал: owned постоянно сбрасывает interpTimer через snapshots.
        entry.isNetworkControlled = entry.isDynamic;

        // Инициализируем снапшоты текущей позицией — чтобы не было рывка с (0,0,0)
        entry.interpFrom   = entry.cachedCFrame;
        entry.interpTo     = entry.cachedCFrame;
        entry.interpTimer  = 0.0f;
        entry.interpPeriod = 0.033f; // начальное значение = ~30 Hz

        entry.dirtyCFrame       = true;
        entry.dirtySize         = true;
        entry.dirtyTransparency = true;

        // --- Подписки ---
        auto& pm = PropertyManager::Get();

        // tokenCFrame — для network-controlled объектов обновляет снапшоты
        // интерполяции, для остальных — как раньше (dirty-флаг).
        entry.tokenCFrame = pm.Subscribe(inst, Classes::BasePart::CFrame,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type != PropertyType::CFrame) return;

                SceneEntry& e = it->second;

                if (e.isNetworkControlled)
                {
                    // Адаптируем interpPeriod под реальный интервал пакетов.
                    // Берём взвешенное скользящее среднее (90% старое, 10% новое)
                    // чтобы не реагировать на одиночные выбросы.
                    float elapsed = m_totalTime - e.lastSnapshotTime;
                    if (e.lastSnapshotTime > 0.0f && elapsed > 0.005f && elapsed < 0.5f)
                        e.interpPeriod = e.interpPeriod * 0.9f + elapsed * 0.1f;
                    e.lastSnapshotTime = m_totalTime;

                    // Старый interpTo становится новым interpFrom.
                    // Сохраняем прогресс: если мы на 70% пути к старому interpTo,
                    // начинаем новый отрезок с той точки, где реально находимся —
                    // это устраняет рывок при приходе нового пакета.
                    float alpha = (e.interpPeriod > 0.0f)
                        ? (e.interpTimer / e.interpPeriod)
                        : 1.0f;
                    if (alpha > 1.0f) alpha = 1.0f;

                    e.interpFrom = LerpCFrame(e.interpFrom, e.interpTo, alpha);
                    e.interpTo   = val.Value.AsCFrame;
                    e.interpTimer = 0.0f;
                }
                else
                {
                    e.cachedCFrame = val.Value.AsCFrame;
                    e.dirtyCFrame  = true;
                }
            });

        entry.tokenSize = pm.Subscribe(inst, Classes::BasePart::Size,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type == PropertyType::Vector3)
                    it->second.cachedSize = val.Value.AsVector3;
                it->second.dirtySize = true;
            });

        entry.tokenTransparency = pm.Subscribe(inst, Classes::BasePart::Transparency,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type == PropertyType::Float)
                    it->second.cachedTransparency = val.Value.AsFloat;
                it->second.dirtyTransparency = true;
            });

        entry.tokenAnchored = pm.Subscribe(inst, Classes::BasePart::Anchored,
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

        entry.tokenColor = pm.Subscribe(inst, Classes::BasePart::Color,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type != PropertyType::Color3) return;
                it->second.cachedColor = val.Value.AsColor3;
                it->second.dirtyColor  = true;
            });

        entry.tokenShape = pm.Subscribe(inst, Classes::BasePart::Shape,
            [this, key](const PropertyValue& val)
            {
                auto it = m_scene.find(key);
                if (it == m_scene.end()) return;
                if (val.Type != PropertyType::Shape) return;
                it->second.cachedShape = val.Value.AsShape;
                it->second.dirtyShape  = true;
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

        if (Classes::IsBasePart(cls))
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
    // Dynamic (Anchored=false, не network-controlled): CFrame из локальной физики.
    // Network-controlled (Anchored=false, позиция с сети): плавная интерполяция
    //   между двумя последними снапшотами — устраняет дёргание от ~30 Hz пакетов.
    // Static (Anchored=true): только по dirty-флагу.
    // -----------------------------------------------------------------------
    void RenderBridge::SyncScene(float deltaTime)
    {
        for (uintptr_t key : m_sceneOrder)
        {
            auto it = m_scene.find(key);
            if (it == m_scene.end()) continue;

            SceneEntry& entry = it->second;
            Instance*   inst  = reinterpret_cast<Instance*>(key);

            // --- Shape — перестраиваем весь меш (смена формы редка) ---
            if (entry.dirtyShape)
            {
                entry.mesh = BuildMesh(entry.cachedShape, entry.cachedColor);
                entry.dirtyShape        = false;
                entry.dirtyColor        = false;
                entry.dirtyCFrame       = true;
                entry.dirtySize         = true;
                entry.dirtyTransparency = true;
                m_decalSyncedParts.erase(key);
                m_surfaceSyncedParts.erase(key);
            }

            // --- Color — перекрашиваем вершины существующего меша ---
            if (entry.dirtyColor)
            {
                SunvoltumRender::Types::Mesh updatedMesh = entry.mesh->GetMesh();
                SunvoltumRender::Types::Color col{
                    entry.cachedColor.R,
                    entry.cachedColor.G,
                    entry.cachedColor.B,
                    1.0f
                };
                std::vector<SunvoltumRender::Types::Vertex> verts = updatedMesh.GetVertices();
                for (auto& v : verts)
                    v.color = col;
                updatedMesh.SetVertices(verts);
                entry.mesh->SetMesh(updatedMesh);
                entry.mesh->SetColor(col);
                entry.dirtyColor = false;
            }

            // --- CFrame ---
            if (entry.isNetworkControlled)
            {
                // Продвигаем таймер и вычисляем alpha ∈ [0, 1].
                // Небольшой overshoot (clamp до 1.2) позволяет объекту
                // «догнать» реальную позицию если пакет опоздал.
                entry.interpTimer += deltaTime;
                float alpha = (entry.interpPeriod > 0.0f)
                    ? (entry.interpTimer / entry.interpPeriod)
                    : 1.0f;
                if (alpha > 1.0f) alpha = 1.0f;

                CFrame rendered = LerpCFrame(entry.interpFrom, entry.interpTo, alpha);
                entry.mesh->SetPosition(ToRender3(rendered.Position));
                entry.mesh->SetRotation(ToMatrix(rendered.Rotation));
            }
            else if (entry.isDynamic)
            {
                // Локальная физика: позиция обновляется каждый тик — применяем напрямую
                entry.mesh->SetPosition(ToRender3(entry.cachedCFrame.Position));
                entry.mesh->SetRotation(ToMatrix(entry.cachedCFrame.Rotation));
            }
            else if (entry.dirtyCFrame)
            {
                // Статический объект: применяем только при изменении
                entry.mesh->SetPosition(ToRender3(entry.cachedCFrame.Position));
                entry.mesh->SetRotation(ToMatrix(entry.cachedCFrame.Rotation));
                entry.dirtyCFrame = false;
            }

            // --- Size ---
            if (entry.dirtySize)
            {
                entry.mesh->SetScale(ToRender3(entry.cachedSize));
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
