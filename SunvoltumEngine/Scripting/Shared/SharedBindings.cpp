#include "SharedBindings.h"
#include "LuauHelpers.h"

#include "../../DataModel/PropertyValue.h"
#include "../../DataModel/InstanceParent.h"
#include "../../DataModel/InstanceClasses/Workspace.h"
#include "../../DataModel/InstanceClasses/ShapePart.h"
#include "../../DataModel/InstanceClasses/Lighting.h"
#include "../../DataModel/InstanceClasses/CurrentCamera.h"
#include "../../DataModel/InstanceClasses/Sound.h"
#include "../../DataModel/InstanceClasses/Decal.h"
#include "../../DataModel/InstanceClasses/TextureSurface.h"
#include "../../DataModel/InstanceClasses/Players.h"
#include "../../DataModel/InstanceClasses/Script.h"
#include "../../DataModel/InstanceClasses/LocalScript.h"
#include "../../DataModel/InstanceClasses/Folder.h"
#include "../../DataModel/InstanceClasses/Model.h"
#include "../../DataModel/InstanceClasses/Motor6D.h"
#include "../../Types/Matrix3x3.h"

#include <cstring>
#include <cmath>
#include <string>

namespace Sunvoltum {
namespace Scripting {
namespace Shared {

// ===========================================================================
//  Helpers
// ===========================================================================

static const char* ClassIdToName(int8_t id)
{
    using namespace Classes;
    switch (id)
    {
    case CLASS_WORKSPACE:      return "Workspace";
    case CLASS_CURRENTCAMERA:  return "CurrentCamera";
    case CLASS_SHAPEPART:      return "ShapePart";
    case CLASS_LIGHTING:       return "Lighting";
    case CLASS_PLAYERS:        return "Players";
    case CLASS_DECAL:          return "Decal";
    case CLASS_TEXTURESURFACE: return "TextureSurface";
    case CLASS_SOUND:          return "Sound";
    case CLASS_SCRIPT:         return "Script";
    case CLASS_LOCALSCRIPT:    return "LocalScript";
    case CLASS_FOLDER:         return "Folder";
    case CLASS_MODEL:          return "Model";
    case CLASS_MOTOR6D:        return "Motor6D";
    default:                   return "Instance";
    }
}

// Читает Sunvoltum::CFrame из таблицы или userdata на позиции idx.
// Если это userdata MT_CFRAME — берёт напрямую.
// Если таблица { X,Y,Z, ... } — не поддерживаем (legacy), возвращаем identity.
static Sunvoltum::CFrame CFrameFromStack(lua_State* L, int idx)
{
    CFrameUD* ud = TestCFrame(L, idx);
    if (ud) return ud->value;
    return Sunvoltum::CFrame::FromPosition(0.0f, 0.0f, 0.0f);
}

// ===========================================================================
//  Instance: TryGetProperty
// ===========================================================================

static bool TryGetProperty(lua_State* L, Instance* inst, const char* key)
{
    using namespace Classes;
    const int8_t cls = inst->GetClassId();

#define PUSH_PROP(PropId) \
    { auto* p = inst->GetProperty(PropId); if (p) PushPropertyValue(L, *p); else lua_pushnil(L); return true; }

    if (cls == CLASS_SHAPEPART)
    {
        if      (!strcmp(key, "CFrame"))       PUSH_PROP(ShapePart::CFrame)
        else if (!strcmp(key, "Size"))         PUSH_PROP(ShapePart::Size)
        else if (!strcmp(key, "Transparency")) PUSH_PROP(ShapePart::Transparency)
        else if (!strcmp(key, "Reflectance"))  PUSH_PROP(ShapePart::Reflectance)
        else if (!strcmp(key, "Color"))        PUSH_PROP(ShapePart::Color)
        else if (!strcmp(key, "Anchored"))     PUSH_PROP(ShapePart::Anchored)
        else if (!strcmp(key, "CanCollide"))   PUSH_PROP(ShapePart::CanCollide)
    }
    else if (cls == CLASS_WORKSPACE)
    {
        if      (!strcmp(key, "Gravity"))        PUSH_PROP(Workspace::Gravity)
        else if (!strcmp(key, "PhysicsEnabled")) PUSH_PROP(Workspace::PhysicsEnabled)
    }
    else if (cls == CLASS_LIGHTING)
    {
        if      (!strcmp(key, "ClockTime"))          PUSH_PROP(Lighting::ClockTime)
        else if (!strcmp(key, "Brightness"))         PUSH_PROP(Lighting::Brightness)
        else if (!strcmp(key, "GeographicLatitude")) PUSH_PROP(Lighting::GeographicLatitude)
    }
    else if (cls == CLASS_CURRENTCAMERA)
    {
        if      (!strcmp(key, "CFrame"))      PUSH_PROP(CurrentCamera::CFrame)
        else if (!strcmp(key, "FieldOfView")) PUSH_PROP(CurrentCamera::FieldOfView)
    }
    else if (cls == CLASS_SOUND)
    {
        if      (!strcmp(key, "Playing"))      PUSH_PROP(Sound::Playing)
        else if (!strcmp(key, "Volume"))       PUSH_PROP(Sound::Volume)
        else if (!strcmp(key, "Looped"))       PUSH_PROP(Sound::Looped)
        else if (!strcmp(key, "SoundId"))      PUSH_PROP(Sound::SoundId)
        else if (!strcmp(key, "TimePosition")) PUSH_PROP(Sound::TimePosition)
    }
    else if (cls == CLASS_DECAL)
    {
        if      (!strcmp(key, "Transparency")) PUSH_PROP(Decal::Transparency)
        else if (!strcmp(key, "Texture"))      PUSH_PROP(Decal::Texture)
    }
    else if (cls == CLASS_TEXTURESURFACE)
    {
        if      (!strcmp(key, "Texture"))       PUSH_PROP(TextureSurface::Texture)
        else if (!strcmp(key, "StudsPerTileU")) PUSH_PROP(TextureSurface::StudsPerTileU)
        else if (!strcmp(key, "StudsPerTileV")) PUSH_PROP(TextureSurface::StudsPerTileV)
    }
    else if (cls == CLASS_MODEL)
    {
        if (!strcmp(key, "PrimaryPart"))
        {
            const PropertyValue* pv = inst->GetProperty(Model::PrimaryPart);
            if (pv && pv->Type == PropertyType::InstanceRef && pv->Value.AsInstanceRef)
                PushInstance(L, pv->Value.AsInstanceRef);
            else
                lua_pushnil(L);
            return true;
        }
    }
    else if (cls == CLASS_MOTOR6D)
    {
        // Part0 / Part1 — возвращаем Instance userdata или nil
        if (!strcmp(key, "Part0") || !strcmp(key, "Part1"))
        {
            PropertyId pid = !strcmp(key, "Part0") ? Motor6D::Part0 : Motor6D::Part1;
            const PropertyValue* pv = inst->GetProperty(pid);
            if (pv && pv->Type == PropertyType::InstanceRef && pv->Value.AsInstanceRef)
                PushInstance(L, pv->Value.AsInstanceRef);
            else
                lua_pushnil(L);
            return true;
        }
        // C0 / C1 — возвращаем CFrame userdata
        if (!strcmp(key, "C0")) PUSH_PROP(Motor6D::C0)
        if (!strcmp(key, "C1")) PUSH_PROP(Motor6D::C1)
    }

#undef PUSH_PROP
    return false;
}

// ===========================================================================
//  Instance: TrySetProperty
//  Значение для записи находится на стеке по индексу 3 (__newindex: self, key, value)
// ===========================================================================

static bool TrySetProperty(lua_State* L, Instance* inst, const char* key)
{
    using namespace Classes;
    const int8_t cls = inst->GetClassId();

    if (cls == CLASS_SHAPEPART)
    {
        if (!strcmp(key, "Transparency"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(ShapePart::Transparency,
                    PropertyValue::Float(static_cast<float>(lua_tonumber(L, 3))));
            return true;
        }
        if (!strcmp(key, "Reflectance"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(ShapePart::Reflectance,
                    PropertyValue::Float(static_cast<float>(lua_tonumber(L, 3))));
            return true;
        }
        if (!strcmp(key, "Anchored"))
        {
            inst->SetProperty(ShapePart::Anchored,
                PropertyValue::Bool(lua_toboolean(L, 3) != 0));
            return true;
        }
        if (!strcmp(key, "CanCollide"))
        {
            inst->SetProperty(ShapePart::CanCollide,
                PropertyValue::Bool(lua_toboolean(L, 3) != 0));
            return true;
        }
        if (!strcmp(key, "Color") && lua_istable(L, 3))
        {
            lua_getfield(L, 3, "R"); float r = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_getfield(L, 3, "G"); float g = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_getfield(L, 3, "B"); float b = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            inst->SetProperty(ShapePart::Color,
                PropertyValue::Color3(Sunvoltum::Color3(r, g, b)));
            return true;
        }
        if (!strcmp(key, "Size") && lua_istable(L, 3))
        {
            lua_getfield(L, 3, "X"); float x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_getfield(L, 3, "Y"); float y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_getfield(L, 3, "Z"); float z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            inst->SetProperty(ShapePart::Size,
                PropertyValue::Vector3(Sunvoltum::Vector3(x, y, z)));
            return true;
        }
        // CFrame — принимаем userdata MT_CFRAME
        if (!strcmp(key, "CFrame"))
        {
            CFrameUD* ud = TestCFrame(L, 3);
            if (ud)
                inst->SetProperty(ShapePart::CFrame,
                    PropertyValue::CFrame(ud->value));
            return true;
        }
    }
    else if (cls == CLASS_WORKSPACE)
    {
        if (!strcmp(key, "Gravity"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(Workspace::Gravity,
                    PropertyValue::Number(lua_tonumber(L, 3)));
            return true;
        }
    }
    else if (cls == CLASS_LIGHTING)
    {
        if (!strcmp(key, "ClockTime"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(Lighting::ClockTime,
                    PropertyValue::Number(lua_tonumber(L, 3)));
            return true;
        }
        if (!strcmp(key, "Brightness"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(Lighting::Brightness,
                    PropertyValue::Number(lua_tonumber(L, 3)));
            return true;
        }
    }
    else if (cls == CLASS_CURRENTCAMERA)
    {
        if (!strcmp(key, "CFrame"))
        {
            CFrameUD* ud = TestCFrame(L, 3);
            if (ud)
                inst->SetProperty(CurrentCamera::CFrame,
                    PropertyValue::CFrame(ud->value));
            return true;
        }
        if (!strcmp(key, "FieldOfView"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(CurrentCamera::FieldOfView,
                    PropertyValue::Number(lua_tonumber(L, 3)));
            return true;
        }
    }
    else if (cls == CLASS_SOUND)
    {
        if (!strcmp(key, "Playing"))
        {
            inst->SetProperty(Sound::Playing,
                PropertyValue::Bool(lua_toboolean(L, 3) != 0));
            return true;
        }
        if (!strcmp(key, "Volume"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(Sound::Volume,
                    PropertyValue::Float(static_cast<float>(lua_tonumber(L, 3))));
            return true;
        }
        if (!strcmp(key, "Looped"))
        {
            inst->SetProperty(Sound::Looped,
                PropertyValue::Bool(lua_toboolean(L, 3) != 0));
            return true;
        }
        if (!strcmp(key, "SoundId"))
        {
            if (lua_isstring(L, 3))
                inst->SetProperty(Sound::SoundId,
                    PropertyValue::String(lua_tostring(L, 3)));
            return true;
        }
    }
    else if (cls == CLASS_DECAL)
    {
        if (!strcmp(key, "Transparency"))
        {
            if (lua_isnumber(L, 3))
                inst->SetProperty(Decal::Transparency,
                    PropertyValue::Float(static_cast<float>(lua_tonumber(L, 3))));
            return true;
        }
        if (!strcmp(key, "Texture"))
        {
            if (lua_isstring(L, 3))
                inst->SetProperty(Decal::Texture,
                    PropertyValue::String(lua_tostring(L, 3)));
            return true;
        }
    }
    else if (cls == CLASS_MODEL)
    {
        if (!strcmp(key, "PrimaryPart"))
        {
            Instance* part = TestInstance(L, 3);
            if (part)
                inst->SetProperty(Model::PrimaryPart, PropertyValue::Ref(part));
            else if (lua_isnil(L, 3))
                inst->SetProperty(Model::PrimaryPart, PropertyValue::Ref(nullptr));
            return true;
        }
    }
    else if (cls == CLASS_MOTOR6D)
    {
        // motor.Part0 = part  /  motor.Part1 = part
        if (!strcmp(key, "Part0") || !strcmp(key, "Part1"))
        {
            PropertyId pid = !strcmp(key, "Part0") ? Motor6D::Part0 : Motor6D::Part1;
            Instance* part = TestInstance(L, 3);
            if (part)
                inst->SetProperty(pid, PropertyValue::Ref(part));
            else if (lua_isnil(L, 3))
                inst->SetProperty(pid, PropertyValue::Ref(nullptr));
            return true;
        }
        // motor.C0 = CFrame  /  motor.C1 = CFrame
        if (!strcmp(key, "C0") || !strcmp(key, "C1"))
        {
            PropertyId pid = !strcmp(key, "C0") ? Motor6D::C0 : Motor6D::C1;
            CFrameUD* ud = TestCFrame(L, 3);
            if (ud)
                inst->SetProperty(pid, PropertyValue::CFrame(ud->value));
            return true;
        }
    }

    return false;
}

// ===========================================================================
//  Model: Luau-методы (возвращаются из __index как closures)
// ===========================================================================

// model:PivotTo(cframe)
static int Model_PivotTo(lua_State* L)
{
    // upvalue 1 = Instance* (model)
    Instance* model = static_cast<Instance*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!model)
    {
        luaL_error(L, "PivotTo: invalid model instance");
        return 0;
    }
    CFrameUD* ud = TestCFrame(L, 1);
    if (!ud)
    {
        luaL_error(L, "PivotTo: argument #1 must be a CFrame");
        return 0;
    }
    Classes::Model::PivotTo(*model, ud->value);
    return 0;
}

// model:SetPrimaryPartCFrame(cframe)
static int Model_SetPrimaryPartCFrame(lua_State* L)
{
    // upvalue 1 = Instance* (model)
    Instance* model = static_cast<Instance*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!model)
    {
        luaL_error(L, "SetPrimaryPartCFrame: invalid model instance");
        return 0;
    }
    CFrameUD* ud = TestCFrame(L, 1);
    if (!ud)
    {
        luaL_error(L, "SetPrimaryPartCFrame: argument #1 must be a CFrame");
        return 0;
    }
    Classes::Model::SetPrimaryPartCFrame(*model, ud->value);
    return 0;
}

// ===========================================================================
//  Instance metatable
// ===========================================================================

static int Instance_Index(lua_State* L)
{
    // [1]=userdata(Instance*), [2]=key
    Instance* inst  = CheckInstance(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (!strcmp(key, "Name"))
    {
        lua_pushstring(L, inst->GetName().c_str());
        return 1;
    }
    if (!strcmp(key, "ClassName"))
    {
        lua_pushstring(L, ClassIdToName(inst->GetClassId()));
        return 1;
    }
    if (!strcmp(key, "Parent"))
    {
        InstanceParent* p = inst->GetParent();
        if (!p)
            lua_pushnil(L);
        else if (auto* pi = dynamic_cast<Instance*>(p))
            PushInstance(L, pi);
        else if (auto* dm = dynamic_cast<DataModel*>(p))
            PushDataModel(L, dm);
        else
            lua_pushnil(L);
        return 1;
    }

    // Дочерний инстанс по имени
    Instance* child = inst->FindByName(key);
    if (child) { PushInstance(L, child); return 1; }

    // Свойство по имени
    if (TryGetProperty(L, inst, key)) return 1;

    // Методы Model
    if (inst->GetClassId() == Classes::CLASS_MODEL)
    {
        if (!strcmp(key, "PivotTo"))
        {
            lua_pushlightuserdata(L, inst);
            lua_pushcclosure(L, Model_PivotTo, "Model.PivotTo", 1);
            return 1;
        }
        if (!strcmp(key, "SetPrimaryPartCFrame"))
        {
            lua_pushlightuserdata(L, inst);
            lua_pushcclosure(L, Model_SetPrimaryPartCFrame, "Model.SetPrimaryPartCFrame", 1);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int Instance_NewIndex(lua_State* L)
{
    // [1]=userdata(Instance*), [2]=key, [3]=value
    Instance* inst  = CheckInstance(L, 1);
    const char* key = luaL_checkstring(L, 2);
    TrySetProperty(L, inst, key);
    return 0;
}

static int Instance_ToString(lua_State* L)
{
    Instance* inst = CheckInstance(L, 1);
    std::string s  = std::string(ClassIdToName(inst->GetClassId()))
                   + "(" + inst->GetName() + ")";
    lua_pushstring(L, s.c_str());
    return 1;
}

static int Instance_Eq(lua_State* L)
{
    Instance* a = CheckInstance(L, 1);
    Instance* b = CheckInstance(L, 2);
    lua_pushboolean(L, a == b ? 1 : 0);
    return 1;
}

static const luaL_Reg s_instanceMeta[] = {
    { "__index",    Instance_Index    },
    { "__newindex", Instance_NewIndex },
    { "__tostring", Instance_ToString },
    { "__eq",       Instance_Eq       },
    { nullptr, nullptr }
};

static void RegisterInstanceMeta(lua_State* L)
{
    luaL_newmetatable(L, MT_INSTANCE);
    luaL_register(L, nullptr, s_instanceMeta);
    lua_pop(L, 1);
}

// ===========================================================================
//  DataModel metatable
// ===========================================================================

static int DataModel_Index(lua_State* L)
{
    DataModel* dm   = CheckDataModel(L, 1);
    const char* key = luaL_checkstring(L, 2);
    Instance* found = dm->FindByName(key);
    if (found) { PushInstance(L, found); return 1; }
    lua_pushnil(L);
    return 1;
}

static int DataModel_ToString(lua_State* L)
{
    lua_pushstring(L, "DataModel");
    return 1;
}

static const luaL_Reg s_dataModelMeta[] = {
    { "__index",    DataModel_Index    },
    { "__tostring", DataModel_ToString },
    { nullptr, nullptr }
};

static void RegisterDataModelMeta(lua_State* L)
{
    luaL_newmetatable(L, MT_DATAMODEL);
    luaL_register(L, nullptr, s_dataModelMeta);
    lua_pop(L, 1);
}

// ===========================================================================
//  CFrame metatable
//
//  Поддерживаемые операции в Luau:
//    cf.Position            → { X, Y, Z } таблица
//    cf.X / cf.Y / cf.Z     → float (позиция)
//    cf.RightVector         → { X, Y, Z }
//    cf.UpVector            → { X, Y, Z }
//    cf.LookVector          → { X, Y, Z }
//    cf * cf2               → CFrame (умножение, комбинирование трансформаций)
//    tostring(cf)           → "CFrame(X, Y, Z)"
// ===========================================================================

static int CFrame_Index(lua_State* L)
{
    // [1]=userdata(CFrameUD), [2]=key
    const Sunvoltum::CFrame& cf = CheckCFrame(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (!strcmp(key, "Position"))
    {
        PushVector3Table(L, cf.Position.X, cf.Position.Y, cf.Position.Z);
        return 1;
    }
    if (!strcmp(key, "X")) { lua_pushnumber(L, static_cast<double>(cf.Position.X)); return 1; }
    if (!strcmp(key, "Y")) { lua_pushnumber(L, static_cast<double>(cf.Position.Y)); return 1; }
    if (!strcmp(key, "Z")) { lua_pushnumber(L, static_cast<double>(cf.Position.Z)); return 1; }

    // Rotation axes (строки Matrix3x3)
    if (!strcmp(key, "RightVector"))
    {
        PushVector3Table(L, cf.Rotation.R00, cf.Rotation.R01, cf.Rotation.R02);
        return 1;
    }
    if (!strcmp(key, "UpVector"))
    {
        PushVector3Table(L, cf.Rotation.R10, cf.Rotation.R11, cf.Rotation.R12);
        return 1;
    }
    if (!strcmp(key, "LookVector"))
    {
        // Roblox/Luau LookVector = -forward = строка 2 матрицы, отрицательная
        PushVector3Table(L, -cf.Rotation.R20, -cf.Rotation.R21, -cf.Rotation.R22);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// CFrame * CFrame → CFrame
static int CFrame_Mul(lua_State* L)
{
    // Оба операнда должны быть CFrame userdata
    CFrameUD* a = TestCFrame(L, 1);
    CFrameUD* b = TestCFrame(L, 2);

    if (!a || !b)
    {
        luaL_error(L, "CFrame.__mul: expected two CFrame values");
        return 0;
    }

    Sunvoltum::CFrame result = a->value * b->value;
    PushCFrame(L, result);
    return 1;
}

static int CFrame_ToString(lua_State* L)
{
    const Sunvoltum::CFrame& cf = CheckCFrame(L, 1);
    char buf[128];
    snprintf(buf, sizeof(buf), "CFrame(%.4g, %.4g, %.4g)",
             static_cast<double>(cf.Position.X),
             static_cast<double>(cf.Position.Y),
             static_cast<double>(cf.Position.Z));
    lua_pushstring(L, buf);
    return 1;
}

static int CFrame_Eq(lua_State* L)
{
    CFrameUD* a = TestCFrame(L, 1);
    CFrameUD* b = TestCFrame(L, 2);
    if (!a || !b) { lua_pushboolean(L, 0); return 1; }
    // Сравниваем только позицию (матрицы сравнивать по float ненадёжно)
    bool eq = (a->value.Position.X == b->value.Position.X)
           && (a->value.Position.Y == b->value.Position.Y)
           && (a->value.Position.Z == b->value.Position.Z);
    lua_pushboolean(L, eq ? 1 : 0);
    return 1;
}

static const luaL_Reg s_cframeMeta[] = {
    { "__index",    CFrame_Index    },
    { "__mul",      CFrame_Mul      },
    { "__tostring", CFrame_ToString },
    { "__eq",       CFrame_Eq       },
    { nullptr, nullptr }
};

static void RegisterCFrameMeta(lua_State* L)
{
    luaL_newmetatable(L, MT_CFRAME);
    luaL_register(L, nullptr, s_cframeMeta);
    lua_pop(L, 1);
}

// ===========================================================================
//  CFrame глобальная таблица
//
//  CFrame.new(x, y, z)                 → CFrame (позиция, без поворота)
//  CFrame.new(x, y, z, r00..r22)       → CFrame (позиция + матрица 3x3, 12 аргументов)
//  CFrame.Angles(pitch, yaw, roll)      → CFrame (только поворот, позиция 0,0,0)
//  CFrame.fromEulerAnglesXYZ(p, y, r)  → алиас Angles
// ===========================================================================

static int CFrame_new(lua_State* L)
{
    int n = lua_gettop(L);

    if (n == 0)
    {
        // CFrame.new() → identity в начале координат
        PushCFrame(L, Sunvoltum::CFrame::FromPosition(0.0f, 0.0f, 0.0f));
        return 1;
    }

    if (n == 3)
    {
        // CFrame.new(x, y, z)
        float x = static_cast<float>(luaL_checknumber(L, 1));
        float y = static_cast<float>(luaL_checknumber(L, 2));
        float z = static_cast<float>(luaL_checknumber(L, 3));
        PushCFrame(L, Sunvoltum::CFrame::FromPosition(x, y, z));
        return 1;
    }

    if (n == 12)
    {
        // CFrame.new(x, y, z,  r00, r01, r02,  r10, r11, r12,  r20, r21, r22)
        float px  = static_cast<float>(luaL_checknumber(L,  1));
        float py  = static_cast<float>(luaL_checknumber(L,  2));
        float pz  = static_cast<float>(luaL_checknumber(L,  3));
        float r00 = static_cast<float>(luaL_checknumber(L,  4));
        float r01 = static_cast<float>(luaL_checknumber(L,  5));
        float r02 = static_cast<float>(luaL_checknumber(L,  6));
        float r10 = static_cast<float>(luaL_checknumber(L,  7));
        float r11 = static_cast<float>(luaL_checknumber(L,  8));
        float r12 = static_cast<float>(luaL_checknumber(L,  9));
        float r20 = static_cast<float>(luaL_checknumber(L, 10));
        float r21 = static_cast<float>(luaL_checknumber(L, 11));
        float r22 = static_cast<float>(luaL_checknumber(L, 12));

        Sunvoltum::Matrix3x3 rot(r00, r01, r02,
                                  r10, r11, r12,
                                  r20, r21, r22);
        Sunvoltum::CFrame cf(Sunvoltum::Vector3(px, py, pz), rot);
        PushCFrame(L, cf);
        return 1;
    }

    luaL_error(L, "CFrame.new: expected 0, 3 or 12 arguments, got %d", n);
    return 0;
}

static int CFrame_Angles(lua_State* L)
{
    // CFrame.Angles(pitch, yaw, roll) — углы в радианах
    // Порядок соответствует Matrix3x3::FromEuler(pitch, yaw, roll)
    float pitch = static_cast<float>(luaL_checknumber(L, 1));
    float yaw   = static_cast<float>(luaL_checknumber(L, 2));
    float roll  = static_cast<float>(luaL_checknumber(L, 3));
    PushCFrame(L, Sunvoltum::CFrame::Angles(pitch, yaw, roll));
    return 1;
}

static void RegisterCFrameGlobal(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, CFrame_new,    "CFrame.new");
    lua_setfield(L, -2, "new");

    lua_pushcfunction(L, CFrame_Angles, "CFrame.Angles");
    lua_setfield(L, -2, "Angles");

    // fromEulerAnglesXYZ — алиас для совместимости
    lua_pushcfunction(L, CFrame_Angles, "CFrame.fromEulerAnglesXYZ");
    lua_setfield(L, -2, "fromEulerAnglesXYZ");

    lua_setglobal(L, "CFrame");
}

// ===========================================================================
//  Instance.new(className, parent?)
//
//  Создаёт новый Instance заданного класса и добавляет его к parent.
//  Если parent не передан — добавляет к DataModel (корню).
//
//  Примеры:
//    local part = Instance.new("ShapePart", workspace)
//    local part = Instance.new("ShapePart")   -- в DataModel
// ===========================================================================

// Маппинг имени класса → ClassId
static int8_t ClassNameToId(const char* name)
{
    using namespace Classes;
    if (!strcmp(name, "ShapePart"))      return CLASS_SHAPEPART;
    if (!strcmp(name, "Workspace"))      return CLASS_WORKSPACE;
    if (!strcmp(name, "Lighting"))       return CLASS_LIGHTING;
    if (!strcmp(name, "CurrentCamera"))  return CLASS_CURRENTCAMERA;
    if (!strcmp(name, "Players"))        return CLASS_PLAYERS;
    if (!strcmp(name, "Decal"))          return CLASS_DECAL;
    if (!strcmp(name, "TextureSurface")) return CLASS_TEXTURESURFACE;
    if (!strcmp(name, "Sound"))          return CLASS_SOUND;
    if (!strcmp(name, "Script"))         return CLASS_SCRIPT;
    if (!strcmp(name, "LocalScript"))    return CLASS_LOCALSCRIPT;
    if (!strcmp(name, "Folder"))         return CLASS_FOLDER;
    if (!strcmp(name, "Model"))          return CLASS_MODEL;
    if (!strcmp(name, "Motor6D"))        return CLASS_MOTOR6D;
    return -1;
}

static int Instance_New(lua_State* L)
{
    // upvalue 1 = DataModel* (используется как родитель по умолчанию)
    DataModel* dm = static_cast<DataModel*>(lua_touserdata(L, lua_upvalueindex(1)));

    const char* className = luaL_checkstring(L, 1);
    int8_t classId = ClassNameToId(className);
    if (classId < 0)
    {
        luaL_error(L, "Instance.new: unknown class \"%s\"", className);
        return 0;
    }

    // Определяем родителя: аргумент 2 (Instance userdata) или DataModel
    InstanceParent* parent = dm;
    Instance* parentInst   = nullptr;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
    {
        parentInst = TestInstance(L, 2);
        if (!parentInst)
        {
            luaL_error(L, "Instance.new: parent must be an Instance");
            return 0;
        }
        parent = parentInst;
    }

    // Создаём инстанс
    Instance& inst = parent->AddInstance(className, classId);

    // Инициализируем свойства по умолчанию для классов с нетривиальным Init
    if (classId == Classes::CLASS_MOTOR6D)
        Classes::Motor6D::Init(inst);

    PushInstance(L, &inst);
    return 1;
}

static void RegisterInstanceGlobal(lua_State* L, DataModel* dm)
{
    lua_newtable(L);

    // Instance.new — upvalue = DataModel*
    lua_pushlightuserdata(L, dm);
    lua_pushcclosure(L, Instance_New, "Instance.new", 1);
    lua_setfield(L, -2, "new");

    lua_setglobal(L, "Instance");
}

// ===========================================================================
//  RegisterSharedBindings — точка входа
// ===========================================================================

void RegisterSharedBindings(lua_State* L)
{
    RegisterInstanceMeta(L);
    RegisterDataModelMeta(L);
    RegisterCFrameMeta(L);
    RegisterCFrameGlobal(L);
    // Instance глобал регистрируется отдельно через RegisterSharedBindingsWithDM
    // чтобы не требовать DataModel в этой функции (вызывается до Init DataModel)
}

// RegisterSharedBindingsWithDM — вызывается после того как DataModel доступен.
// Регистрирует Instance.new с правильным upvalue.
void RegisterSharedBindingsWithDM(lua_State* L, DataModel* dm)
{
    RegisterInstanceGlobal(L, dm);
}

} // namespace Shared
} // namespace Scripting
} // namespace Sunvoltum
