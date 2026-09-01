#include "LuauHelpers.h"

namespace Sunvoltum {
namespace Scripting {
namespace Shared {

// ===========================================================================
//  luaL_testudata — отсутствует в Luau, реализуем вручную.
//  Аналог Lua 5.2: возвращает указатель на userdata если метатаблица совпадает,
//  иначе nullptr (без Lua-ошибки).
// ===========================================================================
static void* SV_testudata(lua_State* L, int idx, const char* tname)
{
    void* p = lua_touserdata(L, idx);
    if (!p) return nullptr;
    if (!lua_getmetatable(L, idx)) return nullptr;   // нет метатаблицы
    luaL_getmetatable(L, tname);                      // ищем нужную
    bool match = (lua_rawequal(L, -1, -2) != 0);
    lua_pop(L, 2);
    return match ? p : nullptr;
}

// ===========================================================================
//  Вспомогательные push
// ===========================================================================

void PushVector3Table(lua_State* L, float x, float y, float z)
{
    lua_newtable(L);
    lua_pushnumber(L, static_cast<double>(x)); lua_setfield(L, -2, "X");
    lua_pushnumber(L, static_cast<double>(y)); lua_setfield(L, -2, "Y");
    lua_pushnumber(L, static_cast<double>(z)); lua_setfield(L, -2, "Z");
}

void PushColor3Table(lua_State* L, float r, float g, float b)
{
    lua_newtable(L);
    lua_pushnumber(L, static_cast<double>(r)); lua_setfield(L, -2, "R");
    lua_pushnumber(L, static_cast<double>(g)); lua_setfield(L, -2, "G");
    lua_pushnumber(L, static_cast<double>(b)); lua_setfield(L, -2, "B");
}

// ===========================================================================
//  Instance
// ===========================================================================

void PushInstance(lua_State* L, Instance* inst)
{
    if (!inst) { lua_pushnil(L); return; }
    Instance** ud = static_cast<Instance**>(lua_newuserdata(L, sizeof(Instance*)));
    *ud = inst;
    luaL_getmetatable(L, MT_INSTANCE);
    lua_setmetatable(L, -2);
}

Instance* CheckInstance(lua_State* L, int idx)
{
    void* ud = luaL_checkudata(L, idx, MT_INSTANCE);
    return *static_cast<Instance**>(ud);
}

Instance* TestInstance(lua_State* L, int idx)
{
    void* ud = SV_testudata(L, idx, MT_INSTANCE);
    return ud ? *static_cast<Instance**>(ud) : nullptr;
}

// ===========================================================================
//  DataModel
// ===========================================================================

void PushDataModel(lua_State* L, DataModel* dm)
{
    DataModel** ud = static_cast<DataModel**>(lua_newuserdata(L, sizeof(DataModel*)));
    *ud = dm;
    luaL_getmetatable(L, MT_DATAMODEL);
    lua_setmetatable(L, -2);
}

DataModel* CheckDataModel(lua_State* L, int idx)
{
    void* ud = luaL_checkudata(L, idx, MT_DATAMODEL);
    return *static_cast<DataModel**>(ud);
}

// ===========================================================================
//  CFrame userdata
// ===========================================================================

void PushCFrame(lua_State* L, const Sunvoltum::CFrame& cf)
{
    CFrameUD* ud = static_cast<CFrameUD*>(lua_newuserdata(L, sizeof(CFrameUD)));
    ud->value = cf;
    luaL_getmetatable(L, MT_CFRAME);
    lua_setmetatable(L, -2);
}

const Sunvoltum::CFrame& CheckCFrame(lua_State* L, int idx)
{
    void* ud = luaL_checkudata(L, idx, MT_CFRAME);
    return static_cast<CFrameUD*>(ud)->value;
}

CFrameUD* TestCFrame(lua_State* L, int idx)
{
    void* ud = SV_testudata(L, idx, MT_CFRAME);
    return static_cast<CFrameUD*>(ud);
}

// ===========================================================================
//  PropertyValue → Luau stack
// ===========================================================================

void PushPropertyValue(lua_State* L, const PropertyValue& pv)
{
    switch (pv.Type)
    {
    case PropertyType::Bool:
        lua_pushboolean(L, pv.Value.AsBool ? 1 : 0);
        break;
    case PropertyType::Int:
        lua_pushnumber(L, static_cast<double>(pv.Value.AsInt));
        break;
    case PropertyType::Float:
        lua_pushnumber(L, static_cast<double>(pv.Value.AsFloat));
        break;
    case PropertyType::Number:
        lua_pushnumber(L, static_cast<double>(pv.Value.AsNumber));
        break;
    case PropertyType::Vector2:
    {
        lua_newtable(L);
        lua_pushnumber(L, static_cast<double>(pv.Value.AsVector2.X)); lua_setfield(L, -2, "X");
        lua_pushnumber(L, static_cast<double>(pv.Value.AsVector2.Y)); lua_setfield(L, -2, "Y");
        break;
    }
    case PropertyType::Vector3:
        PushVector3Table(L, pv.Value.AsVector3.X, pv.Value.AsVector3.Y, pv.Value.AsVector3.Z);
        break;
    case PropertyType::Color3:
        PushColor3Table(L, pv.Value.AsColor3.R, pv.Value.AsColor3.G, pv.Value.AsColor3.B);
        break;
    case PropertyType::CFrame:
        // CFrame свойства отдаём как userdata — поддерживает * и .Position
        PushCFrame(L, pv.Value.AsCFrame);
        break;
    case PropertyType::String:
        lua_pushstring(L, pv.StringValue.c_str());
        break;
    case PropertyType::InstanceRef:
        PushInstance(L, pv.Value.AsInstanceRef);
        break;
    default:
        lua_pushnil(L);
        break;
    }
}

} // namespace Shared
} // namespace Scripting
} // namespace Sunvoltum
