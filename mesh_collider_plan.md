# План: Mesh Colliders в SunvoltumPhysics и SunvoltumPDemo

> PhysX 5.0 предоставляет два типа mesh collider:
> - **PxConvexMeshGeometry** — выпуклый меш, поддерживает динамику (ненулевую массу).
> - **PxTriangleMeshGeometry** — произвольный треугольный меш, только Static / Kinematic.
>
> Именно эту семантику мы воспроизводим.

---

## Текущее состояние кодовой базы

### Что уже есть

| Файл | Статус |
|---|---|
| `Shapes/TriangleMeshShape.h` | Класс есть, но **не подключён к симуляции** |
| `Shapes/ConvexMeshShape.h` | Класс есть, но **зарегистрирован как `ShapeType::Box`** (баг), коллизий нет |
| `ShapeType` enum (`Shape.h`) | Содержит только `Sphere`, `Box`, `Plane`, `Capsule`; `TriangleMesh` и `ConvexMesh` **отсутствуют** |
| `CollisionDispatch` | Нет кейсов для mesh-шейпов |
| `World::Raycast` | Нет поддержки mesh-шейпов |
| BVH / ускоряющая структура | **Отсутствует** |

### Что нужно добавить

1. Исправить `ShapeType` enum.
2. Починить `ConvexMeshShape` (тип).
3. Добавить BVH для `TriangleMeshShape`.
4. Реализовать narrow-phase алгоритмы.
5. Добавить Raycast для обоих типов.
6. Показать использование в `SunvoltumPDemo`.

---

## Шаг 0 — Исправить `ShapeType` enum

**Файл:** `SunvoltumPhysics/include/SunvoltumPhysics/Collision/Shapes/Shape.h`

```cpp
enum class ShapeType {
    Sphere,
    Box,
    Plane,
    Capsule,
    ConvexMesh,      // ← добавить
    TriangleMesh     // ← добавить
};
```

**Файл:** `SunvoltumPhysics/include/SunvoltumPhysics/Collision/Shapes/ConvexMeshShape.h`

Исправить конструктор — заменить `Shape(ShapeType::Box)` на `Shape(ShapeType::ConvexMesh)`.

---

## Шаг 1 — BVH для TriangleMeshShape

Без BVH проверка каждого треугольника — O(n) на запрос. Для сцены с тысячами треугольников это неприемлемо. PhysX строит BVH4 при готовке меша (`PxCooking`). Мы делаем простой AABB-based BVH (бинарное дерево).

### 1.1 Новый файл: `Collision/BVH/TriMeshBVH.h`

```cpp
namespace SunvoltumPhysics {

    struct BVHNode {
        AABB aabb;
        int leftChild  = -1;   // -1 → leaf
        int rightChild = -1;
        int triangleIdx = -1;  // только для листа
    };

    class TriMeshBVH {
    public:
        void Build(const std::vector<Triangle>& triangles);

        // Обход: вызывает callback(triangleIdx) для каждого треугольника, чья AABB
        // пересекает queryAABB
        void Query(const AABB& queryAABB,
                   const std::function<void(int)>& callback) const;

        // Raycast: возвращает ближайшее пересечение с мешем
        bool Raycast(const Vector3& origin, const Vector3& dir, float maxDist,
                     const std::vector<Triangle>& triangles,
                     float& outT, Vector3& outNormal) const;

    private:
        int BuildRecursive(std::vector<int>& indices,
                           const std::vector<AABB>& triAABBs,
                           const std::vector<Vector3>& triCentroids,
                           int begin, int end);

        std::vector<BVHNode> m_nodes;
    };

} // namespace SunvoltumPhysics
```

**Алгоритм построения (SAH-упрощённый, Surface Area Heuristic):**
1. Вычислить AABB для каждого треугольника + AABB всего набора.
2. Найти самую длинную ось этого AABB.
3. Сортировать треугольники по центроиду вдоль этой оси (partition).
4. Рекурсивно разделить на левую и правую половины, пока не остался 1 треугольник (лист).

Это даёт O(n log n) построение и O(log n) средний запрос — достаточно для игрового движка уровня SunvoltumPhysics.

**Новые файлы:**
- `include/SunvoltumPhysics/Collision/BVH/TriMeshBVH.h`
- `src/Collision/BVH/TriMeshBVH.cpp`

### 1.2 Интегрировать BVH в `TriangleMeshShape`

В `TriangleMeshShape.h` добавить:

```cpp
#include "SunvoltumPhysics/Collision/BVH/TriMeshBVH.h"

class TriangleMeshShape : public Shape {
public:
    // ... existing ...
    const TriMeshBVH& GetBVH() const { return m_bvh; }

private:
    void BuildTriangles() {
        // ... existing triangle building ...
        m_bvh.Build(m_triangles);  // ← строить BVH после треугольников
    }

    TriMeshBVH m_bvh;
};
```

---

## Шаг 2 — Narrow-Phase: TriangleMesh коллизии

PhysX обрабатывает `PxTriangleMeshGeometry` только для Static/Kinematic тел. Мы следуем той же семантике.

### 2.1 Вспомогательные алгоритмы (новый файл `src/Collision/TriMeshAlgorithms.cpp`)

Нам нужны две базовые примитивные проверки:

#### Sphere vs Triangle

```
1. Найти ближайшую точку на треугольнике (a, b, c) к центру сферы.
2. Вычислить расстояние до этой точки.
3. Если dist < radius → контакт.
   - normal = (sphereCenter - closestPoint).Normalized()
   - penetration = radius - dist
   - contactPoint = closestPoint
```

#### Box vs Triangle (OBB vs Triangle SAT)

Наиболее трудоёмкая часть. 13 разделяющих осей:
- 3 нормали граней OBB
- 1 нормаль треугольника
- 9 осей от попарного cross(ребро OBB[i], ребро треугольника[j])

Используем тот же SAT-подход, что уже применяется в `BoxVsBox`. Возвращаем минимальный overlap и лучшую ось.

### 2.2 CollisionDispatch: новые методы

В `CollisionDispatch.h` добавить приватные методы:

```cpp
static bool SphereVsTriangleMesh(RigidBody* bodySphere, RigidBody* bodyMesh,
                                  ContactManifold& outManifold);
static bool BoxVsTriangleMesh(RigidBody* bodyBox, RigidBody* bodyMesh,
                               ContactManifold& outManifold);
```

**Логика `SphereVsTriangleMesh`:**
1. Трансформировать AABB сферы в локальное пространство меша.
2. Запросить BVH с этим AABB → список кандидатов-треугольников.
3. Для каждого треугольника: запустить Sphere vs Triangle.
4. Выбрать до 4 контактов (с наибольшим penetration).

**Логика `BoxVsTriangleMesh`:**
1. Трансформировать AABB бокса в мировое пространство → query BVH.
2. Для каждого треугольника-кандидата: OBB vs Triangle SAT.
3. Аккумулировать до 4 лучших контактов.

**Регистрация в `TestCollision`:**

```cpp
// В TestCollision добавить (перед return false):

if (typeA == ShapeType::Sphere && typeB == ShapeType::TriangleMesh) {
    return SphereVsTriangleMesh(bodyA, bodyB, outManifold);
}
if (typeA == ShapeType::TriangleMesh && typeB == ShapeType::Sphere) {
    bool hit = SphereVsTriangleMesh(bodyB, bodyA, outManifold);
    if (hit) { outManifold.bodyA = bodyA; outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal; }
    return hit;
}
if (typeA == ShapeType::Box && typeB == ShapeType::TriangleMesh) {
    return BoxVsTriangleMesh(bodyA, bodyB, outManifold);
}
if (typeA == ShapeType::TriangleMesh && typeB == ShapeType::Box) {
    bool hit = BoxVsTriangleMesh(bodyB, bodyA, outManifold);
    if (hit) { outManifold.bodyA = bodyA; outManifold.bodyB = bodyB;
                outManifold.normal = -outManifold.normal; }
    return hit;
}
```

---

## Шаг 3 — Narrow-Phase: ConvexMesh коллизии

Используем **GJK + EPA** — стандартный подход как в PhysX.

`ConvexMeshShape` уже имеет `GetSupportPoint(localDir)` — это готовая функция поддержки для GJK.

### 3.1 GJK + EPA (`src/Collision/GJK_EPA.cpp`)

**GJK (Gilbert–Johnson–Keerthi):**
- Работает с минковской разностью двух выпуклых тел через их support-функции.
- Определяет: пересекаются ли тела?

**EPA (Expanding Polytope Algorithm):**
- Запускается если GJK сообщает о пересечении.
- Итеративно расширяет симплекс, чтобы найти вектор минимального проникновения.
- Возвращает: normal + penetrationDepth.

```
Новые файлы:
  include/SunvoltumPhysics/Collision/NarrowPhase/GJK_EPA.h
  src/Collision/NarrowPhase/GJK_EPA.cpp
```

Интерфейс:

```cpp
namespace SunvoltumPhysics {
namespace GJK {

    // Возвращает true если тела пересекаются.
    // Заполняет outPenetration и outNormal через EPA.
    bool TestAndGetContact(
        const std::function<Vector3(const Vector3&)>& supportA,   // мировое пространство
        const std::function<Vector3(const Vector3&)>& supportB,
        Vector3& outNormal,
        float& outPenetration,
        Vector3& outContactPoint
    );

} // namespace GJK
} // namespace SunvoltumPhysics
```

Вспомогательные функции support для тел:
- `SphereShape`: `center + dir.Normalized() * radius`
- `BoxShape`: support через OBB vertices (8 точек → max dot)
- `ConvexMeshShape`: уже реализован `GetSupportPoint` + трансформация

### 3.2 CollisionDispatch: ConvexMesh кейсы

```cpp
static bool ConvexMeshVsConvexMesh(RigidBody*, RigidBody*, ContactManifold&);
static bool SphereVsConvexMesh(RigidBody*, RigidBody*, ContactManifold&);
static bool BoxVsConvexMesh(RigidBody*, RigidBody*, ContactManifold&);
```

Все три реализуются через `GJK::TestAndGetContact` с соответствующими support-функциями.

Регистрация — по той же схеме, что в Шаге 2.

---

## Шаг 4 — Raycast для mesh-шейпов

**Файл:** `src/Dynamics/World.cpp`, метод `World::Raycast`.

### TriangleMesh Raycast

```
1. Быстрая проверка: луч vs AABB тела (отсев).
2. Трансформировать луч в локальное пространство меша.
3. Query BVH с AABB луча (bounding box отрезка) → кандидаты.
4. Для каждого треугольника: Möller–Trumbore ray-triangle intersection.
5. Найти минимальный t → ближайшее пересечение.
6. Трансформировать обратно в мировое пространство.
```

**Möller–Trumbore** — стандартный алгоритм:

```cpp
bool RayTriangleMT(const Vector3& orig, const Vector3& dir,
                   const Triangle& tri, float maxDist,
                   float& outT, Vector3& outNormal)
{
    Vector3 e1 = tri.b - tri.a;
    Vector3 e2 = tri.c - tri.a;
    Vector3 h = dir.Cross(e2);
    float  a = e1.Dot(h);
    if (std::abs(a) < EPSILON) return false;      // параллельно
    float  f = 1.0f / a;
    Vector3 s = orig - tri.a;
    float  u = f * s.Dot(h);
    if (u < 0.0f || u > 1.0f) return false;
    Vector3 q = s.Cross(e1);
    float  v = f * dir.Dot(q);
    if (v < 0.0f || u + v > 1.0f) return false;
    float  t = f * e2.Dot(q);
    if (t < EPSILON || t > maxDist) return false;
    outT = t;
    outNormal = tri.normal;
    return true;
}
```

### ConvexMesh Raycast

Используем GJK-based ray-cast (как в PhysX `PxGeometryQuery::raycast`):
- Итеративно сужаем интервал методом поддерживающих гиперплоскостей.
- Либо упрощённый подход: Ray vs каждая грань (плоскость) выпуклого меша + проверка принадлежности.

Для начальной реализации достаточно второго: трансформировать луч в локальное пространство, проверить пересечение со всеми полигонами-гранями, взять ближайшее.

---

## Шаг 5 — ContactManifold: лимит контактов для mesh

У текущего `ContactManifold` жёсткий лимит `MAX_POINTS = 4`. Для TriangleMesh этого достаточно — выбираем 4 контакта с наибольшим penetration, как это делает PhysX (manifold reduction).

Никакого изменения структуры `ContactManifold` не нужно.

---

## Шаг 6 — SunvoltumPDemo: сцена с mesh collider

**Файл:** `SunvoltumPDemo/src/main.cpp`

Добавить демонстрацию двух видов mesh collider.

### 6.1 Генерация mesh сцены

Создать процедурный рампа/горка как `TriangleMeshShape` (два треугольника = наклонная плоскость):

```cpp
#include "SunvoltumPhysics/Collision/Shapes/TriangleMeshShape.h"
#include "SunvoltumPhysics/Collision/Shapes/ConvexMeshShape.h"

// Процедурная рампа (наклонная плоскость 20x20)
auto CreateRampMesh(float width, float depth, float height) {
    // Returns TriangleMeshShape vertices + indices
    // 4 вершины → 2 треугольника
}

// Выпуклый многогранник (восьмигранник как пример)
auto CreateOctahedronMesh(float radius) {
    // Returns ConvexMeshShape vertices
}
```

### 6.2 Спаун mesh-тел в сцене

```cpp
// Статическая рампа из TriangleMeshShape
auto rampVertices = ...;
auto rampIndices  = ...;
auto rampShape = std::make_shared<TriangleMeshShape>(rampVertices, rampIndices);
auto rampBody  = std::make_shared<RigidBody>(BodyType::Static, rampShape,
                    Transform(Vector3(10.0f, 2.0f, 0.0f), ...));
physicsWorld.AddBody(rampBody);

// Динамический выпуклый меш (ConvexMeshShape)
auto octaShape = std::make_shared<ConvexMeshShape>(octaVertices);
auto octaBody  = std::make_shared<RigidBody>(BodyType::Dynamic, octaShape,
                    Transform(Vector3(10.0f, 15.0f, 0.0f), Quaternion(0,0,0,1)));
octaBody->SetMass(2.0f);
physicsWorld.AddBody(octaBody);
```

### 6.3 Визуализация

Сетки рампы и многогранника строятся из тех же вершин/индексов, что и физические шейпы — соответствие 1:1. Синхронизация позиции/ориентации — идентично существующим телам.

### 6.4 Клавиша управления

Добавить клавишу `T` — сбросить сферу на рампу из TriangleMesh:

```cpp
if (input.IsKeyPressed(SunvoltumManager::KeyCode::T)) {
    SpawnSphere(Vector3(10.0f, 20.0f, -5.0f), 1.0f,
                SunvoltumRender::Types::Color(0.2f, 0.9f, 0.3f, 1.0f),
                1.5f, Vector3(0.0f, 0.0f, 5.0f));
}
```

---

## Порядок выполнения

| # | Задача | Файлы | Зависимости |
|---|---|---|---|
| 0 | Исправить `ShapeType` enum + `ConvexMeshShape` тип | `Shape.h`, `ConvexMeshShape.h` | — |
| 1 | BVH для TriangleMesh | `BVH/TriMeshBVH.h/.cpp`, `TriangleMeshShape.h` | Шаг 0 |
| 2a | Sphere vs Triangle алгоритм | `TriMeshAlgorithms.cpp` (новый) | Шаг 1 |
| 2b | OBB vs Triangle SAT | `TriMeshAlgorithms.cpp` | Шаг 1 |
| 2c | Регистрация в CollisionDispatch | `CollisionDispatch.h/.cpp` | Шаг 2a, 2b |
| 3a | GJK + EPA | `GJK_EPA.h/.cpp` | Шаг 0 |
| 3b | ConvexMesh кейсы в CollisionDispatch | `CollisionDispatch.h/.cpp` | Шаг 3a |
| 4 | Raycast для TriangleMesh + ConvexMesh | `World.cpp` | Шаги 1, 3a |
| 5 | Демо сцена в SunvoltumPDemo | `main.cpp` | Шаги 0–4 |

---

## Что намеренно не делаем (из PhysX 5.0)

Следующие возможности PhysX 5.0 мы **не** реализуем в этом плане — они за рамками текущего масштаба:

- **SDF (Signed Distance Field)** для мягкого тела и Contact Weld.
- **Деформируемые меши** (`PxDeformableSurface`).
- **GPU-ускорение** BVH и GJK.
- **Кэширование готовки меша** на диск (PhysX `PxCooking`/Serialization).
- **Trigger-шейпы** (только сенсор, без физического отклика).
- **CCT (Character Controller)** поверх mesh.

Эти функции можно добавить позже как отдельные задачи.

---

## Итог

После выполнения плана:

- `ShapeType::ConvexMesh` и `ShapeType::TriangleMesh` корректно зарегистрированы.
- `TriangleMeshShape` имеет BVH, обеспечивающий эффективную обработку крупных мешей.
- `CollisionDispatch` поддерживает: `Sphere/Box vs TriangleMesh`, `Sphere/Box/Convex vs ConvexMesh`.
- `World::Raycast` работает с обоими типами.
- `SunvoltumPDemo` демонстрирует рампу (TriangleMesh) и динамический многогранник (ConvexMesh).
