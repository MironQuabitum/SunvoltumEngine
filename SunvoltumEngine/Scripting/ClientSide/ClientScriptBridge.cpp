// ClientScriptBridge.cpp
// Полноценный Luau VM для LocalScript на клиенте.
// Архитектура идентична ServerScriptBridge — scheduler, budget, coroutines.
// Добавлено: SetInputSource, FireRenderStepped, FireHeartbeat, FireInputEvents.

#include "ClientScriptBridge.h"
#include "ClientBindings.h"
#include "ClientInstanceBindings.h"
#include "../Shared/SharedBindings.h"
#include "../Shared/LuauHelpers.h"
#include "../../DataModel/InstanceClasses/LocalScript.h"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <sstream>
#include <iostream>
#include <cassert>
#include <algorithm>

namespace Sunvoltum {
namespace Scripting {
namespace Client {

const std::string ClientScriptBridge::s_empty;

// ===========================================================================
//  Singleton
// ===========================================================================

ClientScriptBridge& ClientScriptBridge::Get()
{
    static ClientScriptBridge instance;
    return instance;
}

// ===========================================================================
//  Allocator
// ===========================================================================

static void* LuauClientAlloc(void*, void* ptr, size_t, size_t nsize)
{
    if (nsize == 0) { free(ptr); return nullptr; }
    return realloc(ptr, nsize);
}

// ===========================================================================
//  wait() / task.wait()
// ===========================================================================

static int L_ClientWait(lua_State* L)
{
    double duration = 0.0;
    if (lua_isnumber(L, 1))
        duration = lua_tonumber(L, 1);
    if (duration < 0.0) duration = 0.0;

    auto* bridge = static_cast<ClientScriptBridge*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    lua_pushthread(L);
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);

    bridge->ScheduleWake(L, ref, duration);
    lua_yield(L, 0);
    return 0;
}

// ===========================================================================
//  Interrupt callback — защита от бесконечных циклов
// ===========================================================================

void ClientScriptBridge::OnInterrupt(lua_State* L, int gc)
{
    if (gc) return;

    ClientScriptBridge* bridge =
        static_cast<ClientScriptBridge*>(lua_callbacks(L)->userdata);
    if (!bridge) return;

    if (--bridge->m_budgetCounter <= 0)
        lua_break(L);
}

void ClientScriptBridge::SetupInterruptCallback()
{
    lua_Callbacks* cb = lua_callbacks(m_state);
    cb->userdata  = this;
    cb->interrupt = &ClientScriptBridge::OnInterrupt;
}

// ===========================================================================
//  Init / Shutdown
// ===========================================================================

void ClientScriptBridge::Init(DataModel* dataModel)
{
    assert(!m_state && "ClientScriptBridge::Init called twice");

    m_dataModel = dataModel;
    m_state     = lua_newstate(LuauClientAlloc, nullptr);

    luaL_openlibs(m_state);

    // Общие метатаблицы: Instance, DataModel, CFrame
    Shared::RegisterSharedBindings(m_state);

    // game, workspace, wait(), task
    RegisterGlobals(m_state);

    // Instance.new — требует DataModel
    Shared::RegisterSharedBindingsWithDM(m_state, m_dataModel);

    // UserInputService, RunService, Enum
    RegisterClientBindings(m_state, m_input);

    // instance:SendPropertyUpdate — клиентский метод репликации свойств owned-объектов
    RegisterClientInstanceBindings(m_state);

    SetupInterruptCallback();

    std::cout << "[ClientScriptBridge] Luau VM initialized\n";
}

void ClientScriptBridge::SetInputSource(IInputSource* input)
{
    m_input = input;
    // Если VM уже запущена — переинициализируем UserInputService с реальным input.
    // Проще всего просто пересоздать глобал UserInputService.
    if (m_state)
        RegisterClientBindings(m_state, m_input);
}

void ClientScriptBridge::Shutdown()
{
    if (m_state)
    {
        for (auto& e : m_sleeping)   lua_unref(m_state, e.threadRef);
        for (auto& e : m_interrupted) lua_unref(m_state, e.threadRef);
        m_sleeping.clear();
        m_interrupted.clear();

        lua_close(m_state);
        m_state = nullptr;
    }
    m_scripts.clear();
    m_dataModel   = nullptr;
    m_input       = nullptr;
    m_currentTime = 0.0;
    std::cout << "[ClientScriptBridge] Luau VM shut down\n";
}

// ===========================================================================
//  RegisterGlobals — game, workspace, wait(), task
// ===========================================================================

void ClientScriptBridge::RegisterGlobals(lua_State* L)
{
    using namespace Shared;

    PushDataModel(L, m_dataModel);
    lua_setglobal(L, "game");

    Instance* ws = m_dataModel ? m_dataModel->FindByName("Workspace") : nullptr;
    PushInstance(L, ws);
    lua_setglobal(L, "workspace");

    // wait()
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, L_ClientWait, "wait", 1);
    lua_setglobal(L, "wait");

    // task.wait
    lua_newtable(L);
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, L_ClientWait, "task.wait", 1);
    lua_setfield(L, -2, "wait");
    lua_setglobal(L, "task");

    // warn() — выводит предупреждение, алиас print с префиксом
    lua_pushcfunction(L, [](lua_State* LS) -> int {
        int n = lua_gettop(LS);
        std::cerr << "[WARN]";
        for (int i = 1; i <= n; ++i)
        {
            const char* s = lua_tostring(LS, i);
            std::cerr << " " << (s ? s : "(non-string)");
        }
        std::cerr << "\n";
        return 0;
    }, "warn");
    lua_setglobal(L, "warn");
}

// ===========================================================================
//  Реестр исходников
// ===========================================================================

int ClientScriptBridge::LoadScript(std::istream& stream)
{
    std::ostringstream oss;
    oss << stream.rdbuf();
    return LoadScriptFromSource(oss.str());
}

int ClientScriptBridge::LoadScriptFromSource(const std::string& source)
{
    int id = m_nextId++;
    m_scripts[id] = source;
    return id;
}

bool ClientScriptBridge::HasScript(int scriptId) const
{
    return m_scripts.count(scriptId) > 0;
}

// ===========================================================================
//  RunScript
// ===========================================================================

LocalScriptRunResult ClientScriptBridge::RunScript(int scriptId,
                                                    Instance* scriptInst,
                                                    std::string* outError)
{
    if (!m_state)
    {
        if (outError) *outError = "ClientScriptBridge not initialized";
        return LocalScriptRunResult::RuntimeError;
    }

    auto it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
    {
        if (outError) *outError = "ScriptId not found";
        return LocalScriptRunResult::NotFound;
    }
    const std::string& source = it->second;

    size_t bytecodeSize = 0;
    char*  bytecode     = luau_compile(source.c_str(), source.size(), nullptr, &bytecodeSize);

    if (!bytecode)
    {
        if (outError) *outError = "luau_compile returned null";
        return LocalScriptRunResult::CompileError;
    }
    if (bytecodeSize > 0 && bytecode[0] == 0)
    {
        std::string err(bytecode + 1, bytecodeSize - 1);
        free(bytecode);
        if (outError) *outError = err;
        return LocalScriptRunResult::CompileError;
    }

    lua_State* thread = lua_newthread(m_state);
    int threadRef = lua_ref(m_state, -1);
    lua_pop(m_state, 1);

    std::string chunkName = "LocalScript_" + std::to_string(scriptId);
    int loadStatus = luau_load(thread, chunkName.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);

    if (loadStatus != LUA_OK)
    {
        std::string err = lua_tostring(thread, -1)
                        ? lua_tostring(thread, -1) : "unknown load error";
        lua_unref(m_state, threadRef);
        if (outError) *outError = err;
        return LocalScriptRunResult::CompileError;
    }

    if (scriptInst)
    {
        Shared::PushInstance(thread, scriptInst);
        lua_setglobal(thread, "script");
    }

    m_budgetCounter = kClientInstructionsPerFrame;
    int status = lua_resume(thread, nullptr, 0);

    if (status == LUA_BREAK)
    {
        ClientBudgetEntry be;
        be.thread    = thread;
        be.threadRef = threadRef;
        m_interrupted.push_back(be);
        return LocalScriptRunResult::Ok;
    }

    if (status == LUA_YIELD)
    {
        // Скрипт ушёл в yield через wait() — ScheduleWake уже добавил запись.
        // threadRef будет освобождён когда корутин завершится или будет убран.
        lua_unref(m_state, threadRef);
        return LocalScriptRunResult::Ok;
    }

    if (status != LUA_OK)
    {
        const char* err = lua_tostring(thread, -1);
        std::string msg = err ? err : "unknown runtime error";
        lua_unref(m_state, threadRef);
        if (outError) *outError = msg;
        std::cerr << "[ClientScriptBridge] Runtime error: " << msg << "\n";
        return LocalScriptRunResult::RuntimeError;
    }

    lua_unref(m_state, threadRef);
    return LocalScriptRunResult::Ok;
}

// ===========================================================================
//  Scheduler
// ===========================================================================

void ClientScriptBridge::ScheduleWake(lua_State* thread, int threadRef, double duration)
{
    ClientSleepEntry e;
    e.thread    = thread;
    e.threadRef = threadRef;
    e.wakeAt    = m_currentTime + duration;
    m_sleeping.push_back(e);
}

void ClientScriptBridge::ResumeThread(ClientSleepEntry& entry)
{
    m_budgetCounter = kClientInstructionsPerFrame;

    double elapsed = m_currentTime - entry.wakeAt;
    if (elapsed < 0.0) elapsed = 0.0;

    lua_pushnumber(entry.thread, elapsed);
    int status = lua_resume(entry.thread, nullptr, 1);

    if (status == LUA_BREAK)
    {
        ClientBudgetEntry be;
        be.thread    = entry.thread;
        be.threadRef = entry.threadRef;
        m_interrupted.push_back(be);
        return;
    }

    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* err = lua_tostring(entry.thread, -1);
        std::cerr << "[ClientScriptBridge] Runtime error after wait: "
                  << (err ? err : "unknown") << "\n";
        lua_pop(entry.thread, 1);
    }

    lua_unref(m_state, entry.threadRef);
}

void ClientScriptBridge::ResumeBudgetThread(ClientBudgetEntry& entry)
{
    m_budgetCounter = kClientInstructionsPerFrame;

    int status = lua_resume(entry.thread, nullptr, 0);

    if (status == LUA_BREAK)
    {
        m_interrupted.push_back(entry);
        return;
    }

    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* err = lua_tostring(entry.thread, -1);
        std::cerr << "[ClientScriptBridge] Budget resume error: "
                  << (err ? err : "unknown") << "\n";
        lua_pop(entry.thread, 1);
    }

    lua_unref(m_state, entry.threadRef);
}

void ClientScriptBridge::StepScheduler(double now)
{
    if (!m_state) return;

    m_currentTime   = now;
    m_budgetCounter = kClientInstructionsPerFrame;

    // Продолжаем прерванные по budget корутины
    {
        std::vector<ClientBudgetEntry> toResume;
        toResume.swap(m_interrupted);
        for (auto& e : toResume)
            ResumeBudgetThread(e);
    }

    // Пробуждаем спящие корутины
    std::vector<ClientSleepEntry> toWake;
    toWake.reserve(m_sleeping.size());

    auto newEnd = std::remove_if(m_sleeping.begin(), m_sleeping.end(),
        [now, &toWake](const ClientSleepEntry& e) {
            if (now >= e.wakeAt) { toWake.push_back(e); return true; }
            return false;
        });
    m_sleeping.erase(newEnd, m_sleeping.end());

    for (auto& e : toWake)
        ResumeThread(e);
}

// ===========================================================================
//  Кадровые обновления
// ===========================================================================

void ClientScriptBridge::FireRenderStepped(double dt)
{
    if (!m_state) return;
    FireInputEvents(m_state, m_input);
    Client::FireRenderStepped(m_state, dt);
}

void ClientScriptBridge::FireHeartbeat(double dt)
{
    if (!m_state) return;
    Client::FireHeartbeat(m_state, dt);
}

// ===========================================================================
//  WatchWorkspace — автозапуск LocalScript при добавлении в Workspace
// ===========================================================================

void ClientScriptBridge::WatchWorkspace()
{
    if (!m_dataModel) return;

    Instance* ws = m_dataModel->FindByName("Workspace");
    if (!ws)
    {
        std::cerr << "[ClientScriptBridge] WatchWorkspace: Workspace not found\n";
        return;
    }

    // Подписываемся на ChildAdded у Workspace.
    // Токен хранится в m_watchToken — живёт пока живёт ClientScriptBridge.
    m_watchToken = ws->SubscribeChildAdded([this](Instance& child)
    {
        // Интересуют только LocalScript инстансы
        if (child.GetClassId() != Classes::CLASS_LOCALSCRIPT)
            return;

        // Не запускаем Disabled скрипты
        if (Classes::LocalScript::IsDisabled(child))
            return;

        int scriptId = Classes::LocalScript::GetScriptId(child);
        if (scriptId <= 0)
        {
            std::cerr << "[ClientScriptBridge] LocalScript \""
                      << child.GetName() << "\" has no ScriptId, skipping\n";
            return;
        }

        std::cout << "[ClientScriptBridge] Auto-running LocalScript \""
                  << child.GetName() << "\" (ScriptId=" << scriptId << ")\n";

        std::string err;
        auto result = RunScript(scriptId, &child, &err);
        if (result != LocalScriptRunResult::Ok)
        {
            std::cerr << "[ClientScriptBridge] Error in LocalScript \""
                      << child.GetName() << "\": " << err << "\n";
        }
    });

    std::cout << "[ClientScriptBridge] Watching Workspace for LocalScript\n";
}

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum
