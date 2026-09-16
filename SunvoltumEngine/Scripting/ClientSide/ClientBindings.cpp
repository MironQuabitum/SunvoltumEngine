// ClientBindings.cpp
// Клиентские биндинги Luau:
//   UserInputService  -- обёртка над IInputSource
//   RunService        -- RenderStepped / Heartbeat события
//   Enum              -- KeyCode, MouseButton, UserInputType
//
// Архитектура событий:
//   Connection-объект хранится в Luau registry как lua_ref.
//   Connect(fn) -- сохраняет fn в таблицу коннектов, возвращает Connection.
//   FireRenderStepped(dt) / FireHeartbeat(dt) -- вызываются из C++ каждый кадр,
//   итерируют по таблице и вызывают каждый коннект через lua_pcall.

#include "ClientBindings.h"
#include "../../Runtime/IInputSource.h"
#include "../../Input/KeyCode.h"
#include "../../Input/MouseButton.h"

#include <lua.h>
#include <lualib.h>

#include <cstring>
#include <iostream>

namespace Sunvoltum {
namespace Scripting {
namespace Client {

// ===========================================================================
//  Внутренние ключи registry для хранения таблиц коннектов
// ===========================================================================

static const char* KEY_RENDER_STEPPED = "__svRenderSteppedConns";
static const char* KEY_HEARTBEAT      = "__svHeartbeatConns";
static const char* KEY_INPUT_BEGAN    = "__svInputBeganConns";
static const char* KEY_INPUT_ENDED    = "__svInputEndedConns";
static const char* KEY_PREV_KEYS      = "__svPrevKeys";

// ===========================================================================
//  Вспомогательные функции
// ===========================================================================

static void CreateRegistryTable(lua_State* L, const char* name)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, name);
}

static void GetRegistryTable(lua_State* L, const char* name)
{
    lua_getfield(L, LUA_REGISTRYINDEX, name);
}

// Вызывает все функции в registry-таблице с одним числовым аргументом.
static void FireEventTable(lua_State* L, const char* key, double arg)
{
    GetRegistryTable(L, key);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }

    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        if (lua_isfunction(L, -1))
        {
            lua_pushnumber(L, arg);
            if (lua_pcall(L, 1, 0, 0) != 0)
            {
                const char* err = lua_tostring(L, -1);
                std::cerr << "[ClientBindings] Event error: "
                          << (err ? err : "unknown") << "\n";
                lua_pop(L, 1);
            }
        }
        else
        {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

// Вызывает все функции в registry-таблице с InputObject {UserInputType, KeyCode}.
static void FireInputEvent(lua_State* L, const char* regKey,
                            const char* inputType, int keyCode)
{
    GetRegistryTable(L, regKey);
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }

    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        if (lua_isfunction(L, -1))
        {
            lua_newtable(L);
            lua_pushstring(L, inputType);
            lua_setfield(L, -2, "UserInputType");
            lua_pushinteger(L, keyCode);
            lua_setfield(L, -2, "KeyCode");

            if (lua_pcall(L, 1, 0, 0) != 0)
            {
                const char* err = lua_tostring(L, -1);
                std::cerr << "[ClientBindings] InputEvent error: "
                          << (err ? err : "unknown") << "\n";
                lua_pop(L, 1);
            }
        }
        else
        {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

// ===========================================================================
//  Connection userdata
// ===========================================================================

struct ConnectionUD
{
    const char* regKey;
    int         id;
};

static const char* MT_CONNECTION = "SV_Connection";

static int Connection_Disconnect(lua_State* L)
{
    auto* conn = static_cast<ConnectionUD*>(
        luaL_checkudata(L, 1, MT_CONNECTION));
    if (!conn) return 0;

    GetRegistryTable(L, conn->regKey);
    if (lua_istable(L, -1))
    {
        lua_pushnil(L);
        lua_rawseti(L, -2, conn->id);
    }
    lua_pop(L, 1);
    return 0;
}

static int Connection_ToString(lua_State* L)
{
    lua_pushstring(L, "Connection");
    return 1;
}

static void RegisterConnectionMeta(lua_State* L)
{
    luaL_newmetatable(L, MT_CONNECTION);

    lua_newtable(L);
    lua_pushcfunction(L, Connection_Disconnect, "Connection.Disconnect");
    lua_setfield(L, -2, "Disconnect");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, Connection_ToString, "Connection.__tostring");
    lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);
}

// Добавляет fn в registry-таблицу и возвращает Connection на стек.
// Ожидает fn на позиции fnIdx стека.
static void MakeConnection(lua_State* L, const char* regKey, int fnIdx)
{
    GetRegistryTable(L, regKey);
    int id = static_cast<int>(lua_objlen(L, -1)) + 1;

    lua_pushvalue(L, fnIdx);
    lua_rawseti(L, -2, id);
    lua_pop(L, 1);

    auto* conn = static_cast<ConnectionUD*>(
        lua_newuserdata(L, sizeof(ConnectionUD)));
    conn->regKey = regKey;
    conn->id     = id;
    luaL_getmetatable(L, MT_CONNECTION);
    lua_setmetatable(L, -2);
}

// ===========================================================================
//  Event (RBXScriptSignal-like)  { Connect = fn, _regKey = lightuserdata }
// ===========================================================================

static int Event_Connect(lua_State* L)
{
    // [1]=self, [2]=fn
    if (!lua_isfunction(L, 2))
    {
        luaL_error(L, "Connect: argument #1 must be a function");
        return 0;
    }

    lua_getfield(L, 1, "_regKey");
    const char* regKey = static_cast<const char*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (!regKey)
    {
        luaL_error(L, "Connect: invalid event (no _regKey)");
        return 0;
    }

    MakeConnection(L, regKey, 2);
    return 1;
}

static void PushEvent(lua_State* L, const char* regKey)
{
    lua_newtable(L);
    lua_pushcfunction(L, Event_Connect, "Event.Connect");
    lua_setfield(L, -2, "Connect");
    lua_pushlightuserdata(L, const_cast<char*>(regKey));
    lua_setfield(L, -2, "_regKey");
}

// ===========================================================================
//  Enum.KeyCode / Enum.MouseButton / Enum.UserInputType
// ===========================================================================

static void RegisterEnumGlobal(lua_State* L)
{
    lua_newtable(L); // Enum

    // KeyCode
    lua_newtable(L);
    struct KV { const char* name; int code; };
    static const KV keys[] = {
        {"Unknown",0x00},
        {"Backspace",0x08},{"Tab",0x09},{"Enter",0x0D},
        {"Shift",0x10},{"Control",0x11},{"Alt",0x12},{"Escape",0x1B},
        {"Space",0x20},{"PageUp",0x21},{"PageDown",0x22},
        {"End",0x23},{"Home",0x24},
        {"Left",0x25},{"Up",0x26},{"Right",0x27},{"Down",0x28},
        {"Delete",0x2E},
        {"Zero",0x30},{"One",0x31},{"Two",0x32},{"Three",0x33},{"Four",0x34},
        {"Five",0x35},{"Six",0x36},{"Seven",0x37},{"Eight",0x38},{"Nine",0x39},
        {"A",0x41},{"B",0x42},{"C",0x43},{"D",0x44},{"E",0x45},
        {"F",0x46},{"G",0x47},{"H",0x48},{"I",0x49},{"J",0x4A},
        {"K",0x4B},{"L",0x4C},{"M",0x4D},{"N",0x4E},{"O",0x4F},
        {"P",0x50},{"Q",0x51},{"R",0x52},{"S",0x53},{"T",0x54},
        {"U",0x55},{"V",0x56},{"W",0x57},{"X",0x58},{"Y",0x59},{"Z",0x5A},
        {"F1",0x70},{"F2",0x71},{"F3",0x72},{"F4",0x73},
        {"F5",0x74},{"F6",0x75},{"F7",0x76},{"F8",0x77},
        {"F9",0x78},{"F10",0x79},{"F11",0x7A},{"F12",0x7B},
        {"LeftShift",0xA0},{"RightShift",0xA1},
        {"LeftControl",0xA2},{"RightControl",0xA3},
        {"LeftAlt",0xA4},{"RightAlt",0xA5},
        {"Plus",0xBB},{"Minus",0xBD},
        {nullptr,0}
    };
    for (int i = 0; keys[i].name; ++i)
    {
        lua_pushinteger(L, keys[i].code);
        lua_setfield(L, -2, keys[i].name);
    }
    lua_setfield(L, -2, "KeyCode");

    // UserInputType
    lua_newtable(L);
    lua_pushstring(L, "Keyboard");     lua_setfield(L, -2, "Keyboard");
    lua_pushstring(L, "MouseButton");  lua_setfield(L, -2, "MouseButton");
    lua_pushstring(L, "MouseMovement");lua_setfield(L, -2, "MouseMovement");
    lua_setfield(L, -2, "UserInputType");

    // MouseButton
    lua_newtable(L);
    lua_pushinteger(L, 0); lua_setfield(L, -2, "Left");
    lua_pushinteger(L, 1); lua_setfield(L, -2, "Right");
    lua_pushinteger(L, 2); lua_setfield(L, -2, "Middle");
    lua_setfield(L, -2, "MouseButton");

    lua_setglobal(L, "Enum");
}

// ===========================================================================
//  UserInputService
// ===========================================================================

static int UIS_IsKeyDown(lua_State* L)
{
    auto* input = static_cast<IInputSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!input) { lua_pushboolean(L, 0); return 1; }
    int code = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, input->IsKeyDown(static_cast<KeyCode>(code)) ? 1 : 0);
    return 1;
}

static int UIS_IsKeyUp(lua_State* L)
{
    auto* input = static_cast<IInputSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!input) { lua_pushboolean(L, 1); return 1; }
    int code = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, input->IsKeyDown(static_cast<KeyCode>(code)) ? 0 : 1);
    return 1;
}

static int UIS_IsMouseButtonPressed(lua_State* L)
{
    auto* input = static_cast<IInputSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!input) { lua_pushboolean(L, 0); return 1; }
    int btn = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, input->IsMouseDown(static_cast<MouseButton>(btn)) ? 1 : 0);
    return 1;
}

static int UIS_GetMouseDelta(lua_State* L)
{
    auto* input = static_cast<IInputSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_newtable(L);
    lua_pushnumber(L, input ? static_cast<double>(input->GetMouseDeltaX()) : 0.0);
    lua_setfield(L, -2, "X");
    lua_pushnumber(L, input ? static_cast<double>(input->GetMouseDeltaY()) : 0.0);
    lua_setfield(L, -2, "Y");
    return 1;
}

static int UIS_GetMousePosition(lua_State* L)
{
    auto* input = static_cast<IInputSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_newtable(L);
    lua_pushnumber(L, input ? static_cast<double>(input->GetMouseX()) : 0.0);
    lua_setfield(L, -2, "X");
    lua_pushnumber(L, input ? static_cast<double>(input->GetMouseY()) : 0.0);
    lua_setfield(L, -2, "Y");
    return 1;
}

static int UIS_GetMouseWheelDelta(lua_State* L)
{
    auto* input = static_cast<IInputSource*>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushnumber(L, input ? static_cast<double>(input->GetMouseWheel()) : 0.0);
    return 1;
}

static void PushInputMethod(lua_State* L, lua_CFunction fn,
                             const char* name, IInputSource* input)
{
    lua_pushlightuserdata(L, input);
    lua_pushcclosure(L, fn, name, 1);
}

static void RegisterUserInputService(lua_State* L, IInputSource* input)
{
    lua_newtable(L);

    PushInputMethod(L, UIS_IsKeyDown,            "UIS.IsKeyDown",            input);
    lua_setfield(L, -2, "IsKeyDown");

    PushInputMethod(L, UIS_IsKeyUp,              "UIS.IsKeyUp",              input);
    lua_setfield(L, -2, "IsKeyUp");

    PushInputMethod(L, UIS_IsMouseButtonPressed, "UIS.IsMouseButtonPressed", input);
    lua_setfield(L, -2, "IsMouseButtonPressed");

    PushInputMethod(L, UIS_GetMouseDelta,        "UIS.GetMouseDelta",        input);
    lua_setfield(L, -2, "GetMouseDelta");

    PushInputMethod(L, UIS_GetMousePosition,     "UIS.GetMousePosition",     input);
    lua_setfield(L, -2, "GetMousePosition");

    PushInputMethod(L, UIS_GetMouseWheelDelta,   "UIS.GetMouseWheelDelta",   input);
    lua_setfield(L, -2, "GetMouseWheelDelta");

    lua_pushstring(L, "Default");
    lua_setfield(L, -2, "MouseBehavior");

    PushEvent(L, KEY_INPUT_BEGAN);
    lua_setfield(L, -2, "InputBegan");

    PushEvent(L, KEY_INPUT_ENDED);
    lua_setfield(L, -2, "InputEnded");

    lua_setglobal(L, "UserInputService");
}

// ===========================================================================
//  RunService
// ===========================================================================

static void RegisterRunService(lua_State* L)
{
    lua_newtable(L);

    PushEvent(L, KEY_RENDER_STEPPED);
    lua_setfield(L, -2, "RenderStepped");

    PushEvent(L, KEY_HEARTBEAT);
    lua_setfield(L, -2, "Heartbeat");

    lua_setglobal(L, "RunService");
}

// ===========================================================================
//  Открытые точки входа
// ===========================================================================

void RegisterClientBindings(lua_State* L, IInputSource* input)
{
    RegisterConnectionMeta(L);

    CreateRegistryTable(L, KEY_RENDER_STEPPED);
    CreateRegistryTable(L, KEY_HEARTBEAT);
    CreateRegistryTable(L, KEY_INPUT_BEGAN);
    CreateRegistryTable(L, KEY_INPUT_ENDED);
    CreateRegistryTable(L, KEY_PREV_KEYS);

    RegisterEnumGlobal(L);
    RegisterUserInputService(L, input);
    RegisterRunService(L);

    std::cout << "[ClientBindings] UserInputService and RunService registered\n";
}

void FireRenderStepped(lua_State* L, double dt)
{
    FireEventTable(L, KEY_RENDER_STEPPED, dt);
}

void FireHeartbeat(lua_State* L, double dt)
{
    FireEventTable(L, KEY_HEARTBEAT, dt);
}

void FireInputEvents(lua_State* L, IInputSource* input)
{
    if (!input) return;

    static const int kTrackedKeys[] = {
        0x08,0x09,0x0D,0x10,0x11,0x12,0x1B,0x20,
        0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x2E,
        0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
        0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,
        0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,
        0x55,0x56,0x57,0x58,0x59,0x5A,
        0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,
        0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,
        0xBB,0xBD,
        -1
    };

    GetRegistryTable(L, KEY_PREV_KEYS);

    for (int i = 0; kTrackedKeys[i] >= 0; ++i)
    {
        int code   = kTrackedKeys[i];
        bool nowDown = input->IsKeyDown(static_cast<KeyCode>(code));

        lua_rawgeti(L, -1, code);
        bool wasDown = (lua_toboolean(L, -1) != 0);
        lua_pop(L, 1);

        if (nowDown && !wasDown)
            FireInputEvent(L, KEY_INPUT_BEGAN, "Keyboard", code);
        else if (!nowDown && wasDown)
            FireInputEvent(L, KEY_INPUT_ENDED, "Keyboard", code);

        lua_pushboolean(L, nowDown ? 1 : 0);
        lua_rawseti(L, -2, code);
    }
    lua_pop(L, 1);

    static const char* kBtnPrevKeys[] = { "__svMB0", "__svMB1", "__svMB2" };
    for (int i = 0; i < 3; ++i)
    {
        bool nowDown = input->IsMouseDown(static_cast<MouseButton>(i));

        lua_getfield(L, LUA_REGISTRYINDEX, kBtnPrevKeys[i]);
        bool wasDown = (lua_toboolean(L, -1) != 0);
        lua_pop(L, 1);

        if (nowDown && !wasDown)
            FireInputEvent(L, KEY_INPUT_BEGAN, "MouseButton", i);
        else if (!nowDown && wasDown)
            FireInputEvent(L, KEY_INPUT_ENDED, "MouseButton", i);

        lua_pushboolean(L, nowDown ? 1 : 0);
        lua_setfield(L, LUA_REGISTRYINDEX, kBtnPrevKeys[i]);
    }
}

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum
