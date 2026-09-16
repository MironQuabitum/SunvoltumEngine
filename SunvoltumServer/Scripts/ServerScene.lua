-- ServerScene.lua
-- Инициализация серверной сцены: освещение, геометрия, персонаж, мячи.
-- Запускается через ServerScriptBridge::RunScript() до ServerReplicator::Init.
-- Физическая логика (bounce, LockUpright) остаётся в C++.

local ws       = game.Workspace
local lighting = game.Lighting

-- ===========================================================================
--  Вспомогательные функции
-- ===========================================================================

local function MakePart(parent, name, shape, r, g, b, sx, sy, sz, px, py, pz, anchored, canCollide, transparency)
    local part      = Instance.new("ShapePart", parent)
    part.Name       = name
    part.Shape      = shape
    part.Color      = { R = r, G = g, B = b }
    part.Size       = { X = sx, Y = sy, Z = sz }
    part.CFrame     = CFrame.new(px, py, pz)
    part.Anchored   = anchored
    part.CanCollide = canCollide
    part.Transparency = transparency or 0
    part.PosVelocity  = { X = 0, Y = 0, Z = 0 }
    part.RotVelocity  = { X = 0, Y = 0, Z = 0 }
    return part
end

-- ===========================================================================
--  Workspace
-- ===========================================================================

ws.Gravity        = 196.2
ws.PhysicsEnabled = true

-- ===========================================================================
--  Освещение
-- ===========================================================================

lighting.Brightness         = 2.0
lighting.ClockTime          = 14.0
lighting.GeographicLatitude = 45.0

-- ===========================================================================
--  Пол 95 × 1.5 × 95
-- ===========================================================================

local floor = MakePart(ws, "Floor", "Block",
    0.6, 0.65, 0.7,
    95, 1.5, 95,
    0, 0, 0,
    true, true)

do
    local g = Instance.new("TextureSurface", floor)
    g.Name          = "GrassSurface"
    g.Texture       = "PlatformContent/textures/grass/grass.dds"
    g.StudsPerTileU = 2.25
    g.StudsPerTileV = 2.25
end

-- ===========================================================================
--  Стены
-- ===========================================================================

local fH = 47.5
local wH = 12.0
local wT = 2.0
local wY = 0.75 + wH * 0.5

MakePart(ws, "WallNorth", "Block", 0.45, 0.45, 0.5,  95,       wH, wT,         0,            wY,  fH + wT * 0.5, true, true)
MakePart(ws, "WallSouth", "Block", 0.45, 0.45, 0.5,  95,       wH, wT,         0,            wY, -fH - wT * 0.5, true, true)
MakePart(ws, "WallEast",  "Block", 0.45, 0.45, 0.5,  wT, wH, 95 + wT * 2,  fH + wT * 0.5,  wY,  0,             true, true)
MakePart(ws, "WallWest",  "Block", 0.45, 0.45, 0.5,  wT, wH, 95 + wT * 2, -fH - wT * 0.5,  wY,  0,             true, true)

-- ===========================================================================
--  Персонаж (Character1) — R6 с Weld-суставами
--  HumanoidRootPart — единственное физическое тело (dynamic, CanCollide=true).
--  Все видимые части — dynamic, CanCollide=false, приварены Weld к HRP
--  чтобы следовать за ним в рендере клиента.
-- ===========================================================================

local FLOOR_TOP = 0.75
local HRP_H     = 4.0
local HRP_CY    = FLOOR_TOP + HRP_H * 0.5  -- 2.75
local AX, AZ    = 0, 0

local SK_R, SK_G, SK_B = 0.957, 0.800, 0.263
local LG_R, LG_G, LG_B = 0.647, 0.737, 0.314
local TR_R, TR_G, TR_B = 0.051, 0.412, 0.671

local charModel = Instance.new("Model", ws)
charModel.Name  = "Character1"

-- HumanoidRootPart — двигается физикой, прозрачный, CanCollide=true
local hrp = MakePart(charModel, "HumanoidRootPart", "Block",
    0, 0, 0,
    2, HRP_H, 1,
    AX, HRP_CY, AZ,
    false, true, 1.0)

-- Вспомогательная функция: создать видимую часть и приварить к HRP.
-- localPos — смещение центра части относительно центра HRP.
local function WeldToHRP(name, shape, r, g, b, sx, sy, sz, lx, ly, lz)
    local part = MakePart(charModel, name, shape, r, g, b, sx, sy, sz,
        AX + lx, HRP_CY + ly, AZ + lz,
        false, false)   -- Anchored=false, CanCollide=false

    local w = Instance.new("Weld", charModel)
    w.Name    = "Weld_" .. name
    w.Part0   = hrp
    w.Part1   = part
    w.Enabled = true
    -- C0 — смещение attachment в пространстве HRP (= localPos части)
    -- C1 — identity (attachment в центре части)
    w.C0 = CFrame.new(lx, ly, lz)
    w.C1 = CFrame.new(0,  0,  0)
    return part
end

-- Torso: центр на +1 по Y от центра HRP
WeldToHRP("Torso",     "Block", TR_R, TR_G, TR_B, 2, 2, 1,   0,    1,    0)
-- Ноги: по -1 Y, ±0.5 X
WeldToHRP("Left Leg",  "Block", LG_R, LG_G, LG_B, 1, 2, 1,  -0.5, -1,   0)
WeldToHRP("Right Leg", "Block", LG_R, LG_G, LG_B, 1, 2, 1,   0.5, -1,   0)
-- Руки: на уровне торса, по бокам
WeldToHRP("Left Arm",  "Block", SK_R, SK_G, SK_B, 1, 2, 1,  -1.5,  1,   0)
WeldToHRP("Right Arm", "Block", SK_R, SK_G, SK_B, 1, 2, 1,   1.5,  1,   0)
-- Голова: выше торса
local HEAD_LY = 1 + 1 + 0.8   -- 2.8 от центра HRP
local head = WeldToHRP("Head", "Ball", SK_R, SK_G, SK_B, 1.6, 1.6, 1.6, 0, HEAD_LY, 0)

do
    local face = Instance.new("Decal", head)
    face.Name         = "face"
    face.Texture      = "PlatformContent/textures/EpicFace.dds"
    face.Transparency = 0.0
end

charModel.PrimaryPart = hrp

-- ===========================================================================
--  Humanoid — контроллер персонажа
--  Создаётся ПОСЛЕ всех частей, чтобы PhysicsBridge уже видел HumanoidRootPart
--  и мог применить LockUpright при подписке ChildAdded.
-- ===========================================================================

local humanoid = Instance.new("Humanoid", charModel)
humanoid.Name       = "Humanoid"
humanoid.Health     = 100
humanoid.MaxHealth  = 100
humanoid.WalkSpeed  = 16
humanoid.JumpPower  = 50
humanoid.HipHeight  = 0.1

-- ===========================================================================
--  20 прыгающих мячей
-- ===========================================================================

local BALL_COLORS = {
    {0.95, 0.20, 0.20}, {0.95, 0.55, 0.10}, {0.95, 0.90, 0.10}, {0.40, 0.85, 0.20},
    {0.10, 0.75, 0.75}, {0.20, 0.40, 0.90}, {0.65, 0.20, 0.90}, {0.90, 0.20, 0.65},
    {0.90, 0.90, 0.90}, {0.30, 0.30, 0.30}, {0.85, 0.50, 0.30}, {0.20, 0.85, 0.50},
    {0.50, 0.20, 0.10}, {0.10, 0.50, 0.85}, {0.80, 0.80, 0.10}, {0.10, 0.80, 0.30},
    {0.80, 0.10, 0.10}, {0.60, 0.60, 0.95}, {0.95, 0.60, 0.80}, {0.30, 0.80, 0.80},
}

local COLS    = 5
local ROWS    = 4
local SPACING = 9
local BALL_D  = 3
local START_X = -(COLS - 1) * SPACING * 0.5
local START_Z = -(ROWS - 1) * SPACING * 0.5

local idx = 0
for row = 0, ROWS - 1 do
    for col = 0, COLS - 1 do
        idx = idx + 1
        local bx = START_X + col * SPACING
        local bz = START_Z + row * SPACING
        local by = 5 + (idx - 1) * 1.2
        local c  = BALL_COLORS[idx]
        local initVY = 30 + ((idx - 1) % 5) * 8

        local ball = MakePart(ws, "Ball" .. idx, "Ball",
            c[1], c[2], c[3],
            BALL_D, BALL_D, BALL_D,
            bx, by, bz,
            false, true)
        ball.PosVelocity = { X = 0, Y = initVY, Z = 0 }
    end
end

-- ===========================================================================
--  BlueBall
-- ===========================================================================

MakePart(ws, "BlueBall", "Ball",
    0.10, 0.35, 0.95,
    4, 4, 4,
    -30, FLOOR_TOP + 2, 0,
    false, true)

-- ===========================================================================
--  Стул — 6 частей, скреплены Weld, падает с высоты 5 стадов
--
--  Система координат (все смещения в локальном пространстве сидения):
--
--    Сидение (Seat)    — 2 × 0.3 × 2,  центр = мировой origin стула
--    Спинка (Back)     — 2 × 1.8 × 0.2, сзади и выше сидения
--    Ножки (Leg*)      — 0.3 × 1.5 × 0.3, под углами сидения
--
--  Стул поставлен на y=5 (дно ножек = 5 - 1.5/2 = 4.25 студа над полом)
-- ===========================================================================

do
    local CHAIR_X, CHAIR_Z = 10, 0       -- мировое положение стула
    local DROP_Y = 5                      -- высота центра сидения при старте

    -- Цвета
    local WR, WG, WB = 0.55, 0.27, 0.07  -- коричневый (дерево)

    -- Сидение — главная часть, не Anchored, к ней всё крепится
    local seat = MakePart(ws, "ChairSeat", "Block",
        WR, WG, WB,
        2.0, 0.3, 2.0,
        CHAIR_X, DROP_Y, CHAIR_Z,
        false, true)
    seat.PosVelocity = { X = 0, Y = 46, Z = 0 }  -- начальный импульс вверх, как у мячей

    -- Вспомогательная функция: создаёт часть и приваривает её к сидению.
    -- localPos — смещение центра новой части относительно центра сидения.
    local function WeldPart(name, sx, sy, sz, lx, ly, lz)
        local part = MakePart(ws, name, "Block",
            WR, WG, WB,
            sx, sy, sz,
            CHAIR_X + lx, DROP_Y + ly, CHAIR_Z + lz,
            false, true)

        local w   = Instance.new("Weld", ws)
        w.Name    = "Weld_" .. name
        w.Part0   = seat
        w.Part1   = part
        w.Enabled = true

        w.C0 = CFrame.new(lx, ly, lz)
        w.C1 = CFrame.new(0, 0, 0)
    end

    -- Спинка: позади сидения на z = -0.9, поднята на 1.05 от центра сидения
    WeldPart("ChairBack",
        2.0, 1.8, 0.2,
         0,   0.9 + 0.15,  -0.9)

    -- 4 ножки: по углам сидения, опущены вниз на 0.75 + 0.15 = 0.9 от центра
    local legDX =  0.85  -- половина сидения по X минус половина ножки
    local legDZ =  0.85  -- то же по Z
    local legDY = -(0.3 * 0.5 + 1.5 * 0.5)  -- -0.9 (ниже нижней грани сидения)

    WeldPart("ChairLegFL", 0.3, 1.5, 0.3,  legDX, legDY,  legDZ)
    WeldPart("ChairLegFR", 0.3, 1.5, 0.3, -legDX, legDY,  legDZ)
    WeldPart("ChairLegBL", 0.3, 1.5, 0.3,  legDX, legDY, -legDZ)
    WeldPart("ChairLegBR", 0.3, 1.5, 0.3, -legDX, legDY, -legDZ)
end

-- ===========================================================================
--  Пропеллер
--
--  Структура (снизу вверх):
--    PropBase  — основа 2×1×2, Anchored, стоит на полу
--    PropShaft — вал 0.5×4×0.5, dynamic, крутится Motor6D вокруг Y-оси
--    PropBlade — лопасть 4×0.5×0.5, Weld к валу (наверху вала)
--
--  Motor6D: Part0=PropBase, Part1=PropShaft
--    C0 = верхняя грань основы (+0.5 по Y) + поворот 90° по Z
--    C1 = нижняя грань вала    (-2.0 по Y) + поворот 90° по Z
--    Поворот joint frame на 90° по Z переводит ось eTWIST (X) в мировую Y.
--
--  Цикл: каждые STEP_S секунд прибавляем SPEED * STEP_S к DesiredAngle.
--  PhysicsBridge::SyncJointsDrive видит разницу (DesiredAngle - CurrentAngle) > 0
--  и выставляет DriveVelocity = +MaxVelocity → вал крутится непрерывно.
--
--  Weld: Part0=PropShaft, Part1=PropBlade
-- ===========================================================================

do
    local PX, PZ    = -15, 10
    local FLOOR_TOP = 0.75

    -- Основа: 2×1×2, Anchored
    local baseY    = FLOOR_TOP + 0.5        -- центр основы: 1.25
    local propBase = MakePart(ws, "PropBase", "Block",
        0.40, 0.40, 0.45,
        2, 1, 2,
        PX, baseY, PZ,
        true, true)

    -- Вал: 0.5×4×0.5, dynamic, CanCollide=false
    local shaftY    = baseY + 0.5 + 2.0     -- 3.75
    local propShaft = MakePart(ws, "PropShaft", "Block",
        0.45, 0.45, 0.50,
        0.5, 4, 0.5,
        PX, shaftY, PZ,
        false, false)

    -- Motor6D: PropBase → PropShaft
    -- Поворот joint frame на 90° по Z: ось eTWIST (X) совпадает с мировой Y
    local halfPi = math.pi * 0.5
    local motor  = Instance.new("Motor6D", ws)
    motor.Name         = "PropMotor"
    motor.Part0        = propBase
    motor.Part1        = propShaft
    motor.Enabled      = true
    motor.C0           = CFrame.new(0,  0.5, 0) * CFrame.Angles(0, 0, halfPi)
    motor.C1           = CFrame.new(0, -2.0, 0) * CFrame.Angles(0, 0, halfPi)
    motor.MaxVelocity  = 3.0   -- рад/с

    -- Лопасть: 4×0.5×0.5, Weld к верхушке вала
    local bladeY    = shaftY + 2.0           -- 5.75
    local propBlade = MakePart(ws, "PropBlade", "Block",
        0.20, 0.60, 0.90,
        4, 0.5, 0.5,
        PX, bladeY, PZ,
        false, false)

    local bladeWeld   = Instance.new("Weld", ws)
    bladeWeld.Name    = "PropBladeWeld"
    bladeWeld.Part0   = propShaft
    bladeWeld.Part1   = propBlade
    bladeWeld.Enabled = true
    bladeWeld.C0      = CFrame.new(0,  2.0, 0)
    bladeWeld.C1      = CFrame.new(0,  0.0, 0)

    -- Цикл вращения запускается из C++ (main.cpp PreSimulation).
    -- DesiredAngle обновляется каждый тик: motor.DesiredAngle = motor.CurrentAngle + step.
    -- Здесь только инициализируем начальное значение.
    motor.DesiredAngle = 0.0
end

print("[ServerScene] Scene built.")
