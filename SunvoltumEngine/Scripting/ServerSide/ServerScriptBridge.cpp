#include "ServerScriptBridge.h"
#include "LuauBindings.h"
#include "../../DataModel/InstanceClasses/Script.h"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>   // luau_compile

#include <sstream>
#include <iostream>
#include <cassert>
#include <algorithm>

namespace Sunvoltum {
namespace Scripting {
namespace Server {

const std::string ServerScriptBridge::s_empty;

// ===========================================================================
//  Singleton
// ===========================================================================

ServerScriptBridge& ServerScriptBridge::Get()
{
    static ServerScriptBridge instance;
    return instance;
}

// ===========================================================================
//  Allocator
// ===========================================================================

static void* LuauAlloc(void* /*ud*/, void* ptr, size_t /*osize*/, size_t nsize)
{
    if (nsize == 0) { free(ptr); return nullptr; }
    return realloc(ptr, nsize);
}

// ===========================================================================
//  wait() / task.wait()
//
//  Сигнатура Luau: wait(seconds?: number) → number  (возвращает реально прошедшее время)
//
//  Принцип:
//    1. C-функция читает аргумент (по умолчанию 0 — yield до следующего кадра).
//    2. Вычисляет wakeAt = currentTime + duration.
//    3. Регистрирует entry через ScheduleWake.
//    4. Делает lua_yield → управление возвращается в RunScript/StepScheduler.
//    5. StepScheduler при пробуждении вызывает lua_resume с одним аргументом —
//       реально прошедшим временем (double), — скрипт его получает как результат wait().
// ===========================================================================

static int L_Wait(lua_State* L)
{
    double duration = 0.0;
    if (lua_isnumber(L, 1))
        duration = lua_tonumber(L, 1);
    if (duration < 0.0) duration = 0.0;

    // Получаем bridge через upvalue (зарегистрировано при Init)
    auto* bridge = static_cast<ServerScriptBridge*>(lua_touserdata(L, lua_upvalueindex(1)));

    // Создаём ref до yield — после yield L уже недостаточно для ref
    lua_pushthread(L);                          // кладём сам thread на стек
    int ref = lua_ref(L, -1);                   // ref в registry этого же thread
    lua_pop(L, 1);

    bridge->ScheduleWake(L, ref, duration);

    // Yield: возвращаем 0 значений сейчас — реальное время передадим при resume
    lua_yield(L, 0);
    return 0;
}

// ===========================================================================
//  Init / Shutdown
// ===========================================================================

void ServerScriptBridge::Init(DataModel* dataModel)
{
    assert(!m_state && "ServerScriptBridge::Init called twice");

    m_dataModel = dataModel;

    m_state = lua_newstate(LuauAlloc, nullptr);

    // Безопасное подмножество стандартных библиотек (без io/os/package)
    luaL_openlibs(m_state);

    // Регистрируем все общие метатаблицы и CFrame-глобал (Instance, DataModel, CFrame)
    Shared::RegisterSharedBindings(m_state);

    // Глобалы: game, workspace, wait(), task
    RegisterGlobals(m_state);

    // Instance.new — требует DataModel, регистрируем после RegisterGlobals
    Shared::RegisterSharedBindingsWithDM(m_state, m_dataModel);

    // Регистрируем interrupt callback для защиты от бесконечных циклов
    SetupInterruptCallback();

    std::cout << "[ServerScriptBridge] Luau VM initialized\n";
}

void ServerScriptBridge::Shutdown()
{
    if (m_state)
    {
        // Освобождаем ref'ы спящих корутин
        for (auto& e : m_sleeping)
            lua_unref(m_state, e.threadRef);
        m_sleeping.clear();

        // Освобождаем ref'ы корутин, прерванных по budget
        for (auto& e : m_interrupted)
            lua_unref(m_state, e.threadRef);
        m_interrupted.clear();

        lua_close(m_state);
        m_state = nullptr;
    }
    m_scripts.clear();
    m_dataModel = nullptr;
    m_currentTime = 0.0;
    std::cout << "[ServerScriptBridge] Luau VM shut down\n";
}

// ===========================================================================
//  RegisterGlobals
//  Регистрирует: game, workspace, wait(), task
// ===========================================================================

void ServerScriptBridge::RegisterGlobals(lua_State* L)
{
    // game → DataModel userdata
    PushDataModel(L, m_dataModel);
    lua_setglobal(L, "game");

    // workspace → Workspace инстанс (shortcut)
    Instance* ws = m_dataModel ? m_dataModel->FindByName("Workspace") : nullptr;
    PushInstance(L, ws);
    lua_setglobal(L, "workspace");

    // wait(n) — глобальная функция, upvalue = this
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, L_Wait, "wait", 1);
    lua_setglobal(L, "wait");

    // task — таблица { wait = function }
    lua_newtable(L);

    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, L_Wait, "task.wait", 1);
    lua_setfield(L, -2, "wait");

    lua_setglobal(L, "task");
}

// ===========================================================================
//  Реестр исходников
// ===========================================================================

int ServerScriptBridge::LoadScript(std::istream& stream)
{
    std::ostringstream oss;
    oss << stream.rdbuf();
    return LoadScriptFromSource(oss.str());
}

int ServerScriptBridge::LoadScriptFromSource(const std::string& source)
{
    int id = m_nextId++;
    m_scripts[id] = source;
    return id;
}

const std::string& ServerScriptBridge::GetSource(int scriptId) const
{
    auto it = m_scripts.find(scriptId);
    return (it != m_scripts.end()) ? it->second : s_empty;
}

bool ServerScriptBridge::HasScript(int scriptId) const
{
    return m_scripts.count(scriptId) > 0;
}

void ServerScriptBridge::UnloadScript(int scriptId)
{
    m_scripts.erase(scriptId);
}

// ===========================================================================
//  ScheduleWake — добавить thread в очередь ожидания
// ===========================================================================

void ServerScriptBridge::ScheduleWake(lua_State* thread, int threadRef, double duration)
{
    SleepEntry entry;
    entry.thread    = thread;
    entry.threadRef = threadRef;
    entry.wakeAt    = m_currentTime + duration;
    m_sleeping.push_back(entry);
}

// ===========================================================================
//  Interrupt callback — вызывается Luau на каждом safepoint
//  (loop back edges, call/ret, gc).
//
//  Параметр gc != 0 означает что вызов произошёл из GC — в этом случае
//  трогать стек нельзя, просто выходим.
// ===========================================================================

/*static*/
void ServerScriptBridge::OnInterrupt(lua_State* L, int gc)
{
    if (gc) return; // вызов из GC — не трогаем стек

    // Получаем bridge из userdata callbacks
    ServerScriptBridge* bridge =
        static_cast<ServerScriptBridge*>(lua_callbacks(L)->userdata);
    if (!bridge) return;

    if (--bridge->m_budgetCounter <= 0)
    {
        // Бюджет исчерпан — прерываем исполнение текущего корутина.
        // lua_break возвращает LUA_BREAK из lua_resume, не убивает корутин.
        lua_break(L);
    }
}

void ServerScriptBridge::SetupInterruptCallback()
{
    lua_Callbacks* cb = lua_callbacks(m_state);
    cb->userdata  = this;
    cb->interrupt = &ServerScriptBridge::OnInterrupt;
}

// ===========================================================================
//  ResumeBudgetThread — возобновить корутин, прерванный по budget
// ===========================================================================

void ServerScriptBridge::ResumeBudgetThread(BudgetEntry& entry)
{
    // Сбрасываем бюджет перед продолжением этого корутина
    m_budgetCounter = kInstructionsPerFrame;

    int status = lua_resume(entry.thread, nullptr, 0);

    if (status == LUA_BREAK)
    {
        // Снова исчерпал бюджет — оставляем в очереди на следующий кадр
        m_interrupted.push_back(entry);
        return;
    }

    if (status == LUA_YIELD)
    {
        // Вызвал wait() — ScheduleWake уже поставил его в m_sleeping,
        // новый ref там. Освобождаем старый.
        lua_unref(m_state, entry.threadRef);
        return;
    }

    if (status != LUA_OK)
    {
        const char* err = lua_tostring(entry.thread, -1);
        std::cerr << "[ServerScriptBridge] Runtime error (budget resume): "
                  << (err ? err : "unknown") << "\n";
        lua_pop(entry.thread, 1);
    }

    lua_unref(m_state, entry.threadRef);
}


void ServerScriptBridge::ResumeThread(SleepEntry& entry)
{
    // Сбрасываем бюджет перед каждым возобновлением
    m_budgetCounter = kInstructionsPerFrame;

    // Передаём реально прошедшее время как аргумент resume → результат wait()
    double elapsed = m_currentTime - entry.wakeAt;  // wakeAt уже прошёл
    if (elapsed < 0.0) elapsed = 0.0;

    lua_pushnumber(entry.thread, elapsed);
    int status = lua_resume(entry.thread, nullptr, 1);

    if (status == LUA_BREAK)
    {
        // Корутин исчерпал бюджет прямо после пробуждения — откладываем на следующий кадр
        BudgetEntry be;
        be.thread    = entry.thread;
        be.threadRef = entry.threadRef; // передаём владение ref
        m_interrupted.push_back(be);
        return;
    }

    if (status != LUA_OK && status != LUA_YIELD)
    {
        const char* err = lua_tostring(entry.thread, -1);
        std::cerr << "[ServerScriptBridge] Runtime error after wait resume: "
                  << (err ? err : "unknown") << "\n";
        lua_pop(entry.thread, 1);
    }

    // Если корутин снова сделал yield — он уже зарегистрировал себя заново
    // через ScheduleWake, так что ref будет новым. Старый ref освобождаем.
    lua_unref(m_state, entry.threadRef);
}

// ===========================================================================
//  StepScheduler — вызывается каждый кадр из Heartbeat
// ===========================================================================

void ServerScriptBridge::StepScheduler(double now)
{
    if (!m_state) return;

    m_currentTime = now;

    // Сбрасываем instruction budget для нового кадра
    m_budgetCounter = kInstructionsPerFrame;

    // Возобновляем корутины, прерванные в прошлом кадре из-за budget.
    // Итерируем по копии: ResumeBudgetThread может добавить новые в m_interrupted.
    {
        std::vector<BudgetEntry> toResume;
        toResume.swap(m_interrupted);
        for (auto& entry : toResume)
            ResumeBudgetThread(entry);
    }

    // Собираем корутины у которых время пришло
    // Итерируем по копии чтобы ScheduleWake внутри ResumeThread мог добавить новые
    std::vector<SleepEntry> toWake;
    toWake.reserve(m_sleeping.size());

    auto newEnd = std::remove_if(m_sleeping.begin(), m_sleeping.end(),
        [now, &toWake](const SleepEntry& e) {
            if (now >= e.wakeAt) {
                toWake.push_back(e);
                return true;
            }
            return false;
        });
    m_sleeping.erase(newEnd, m_sleeping.end());

    for (auto& entry : toWake)
        ResumeThread(entry);
}

// ===========================================================================
//  RunScript
// ===========================================================================

ScriptRunResult ServerScriptBridge::RunScript(int scriptId,
                                               Instance* scriptInst,
                                               std::string* outError)
{
    if (!m_state)
    {
        if (outError) *outError = "ServerScriptBridge not initialized";
        return ScriptRunResult::RuntimeError;
    }

    auto it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
    {
        if (outError) *outError = "ScriptId " + std::to_string(scriptId) + " not found";
        return ScriptRunResult::NotFound;
    }
    const std::string& source = it->second;

    // Компилируем → bytecode
    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(source.c_str(), source.size(), nullptr, &bytecodeSize);

    if (!bytecode)
    {
        if (outError) *outError = "luau_compile returned null";
        return ScriptRunResult::CompileError;
    }
    if (bytecodeSize > 0 && bytecode[0] == 0)
    {
        std::string errMsg(bytecode + 1, bytecodeSize - 1);
        free(bytecode);
        if (outError) *outError = errMsg;
        return ScriptRunResult::CompileError;
    }

    // Создаём coroutine-thread
    lua_State* thread = lua_newthread(m_state);
    int threadRef = lua_ref(m_state, -1);
    lua_pop(m_state, 1);

    // Загружаем bytecode
    std::string chunkName = "Script_" + std::to_string(scriptId);
    int loadStatus = luau_load(thread, chunkName.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);

    if (loadStatus != LUA_OK)
    {
        std::string errMsg = lua_tostring(thread, -1) ? lua_tostring(thread, -1) : "unknown load error";
        lua_unref(m_state, threadRef);
        if (outError) *outError = errMsg;
        return ScriptRunResult::CompileError;
    }

    // Регистрируем глобал script для этого thread
    PushInstance(thread, scriptInst);
    lua_setglobal(thread, "script");

    // Сбрасываем бюджет перед запуском нового скрипта
    m_budgetCounter = kInstructionsPerFrame;

    // Запускаем
    int resumeStatus = lua_resume(thread, nullptr, 0);

    if (resumeStatus == LUA_BREAK)
    {
        // Скрипт исчерпал бюджет при первом же запуске (например, while true do end).
        // Откладываем продолжение на следующий кадр.
        BudgetEntry be;
        be.thread    = thread;
        be.threadRef = threadRef; // передаём владение ref
        m_interrupted.push_back(be);
        return ScriptRunResult::Ok;
    }

    if (resumeStatus == LUA_YIELD)
    {
        // Скрипт вызвал wait() — thread живёт в m_sleeping, ref уже там.
        // threadRef из lua_ref выше тоже нужно освободить — ScheduleWake создал свой ref.
        lua_unref(m_state, threadRef);
        return ScriptRunResult::Ok;
    }

    if (resumeStatus != LUA_OK)
    {
        const char* rawErr = lua_tostring(thread, -1);
        std::string errMsg = rawErr ? rawErr : "unknown runtime error";
        lua_pop(thread, 1);

        std::cerr << "[ServerScriptBridge] Runtime error in Script_"
                  << scriptId << ": " << errMsg << "\n";

        lua_unref(m_state, threadRef);
        if (outError) *outError = errMsg;
        return ScriptRunResult::RuntimeError;
    }

    lua_unref(m_state, threadRef);
    return ScriptRunResult::Ok;
}

} // namespace Server
} // namespace Scripting
} // namespace Sunvoltum

// ===========================================================================
//  WatchWorkspace — автозапуск Script при добавлении в Workspace
// ===========================================================================

namespace Sunvoltum { namespace Scripting { namespace Server {

void ServerScriptBridge::WatchWorkspace()
{
    if (!m_dataModel) return;

    Instance* ws = m_dataModel->FindByName("Workspace");
    if (!ws)
    {
        std::cerr << "[ServerScriptBridge] WatchWorkspace: Workspace not found\n";
        return;
    }

    m_watchToken = ws->SubscribeChildAdded([this](Instance& child)
    {
        if (child.GetClassId() != Classes::CLASS_SCRIPT)
            return;

        if (Classes::Script::IsDisabled(child))
            return;

        int scriptId = Classes::Script::GetScriptId(child);
        if (scriptId <= 0)
        {
            std::cerr << "[ServerScriptBridge] Script \""
                      << child.GetName() << "\" has no ScriptId, skipping\n";
            return;
        }

        std::cout << "[ServerScriptBridge] Auto-running Script \""
                  << child.GetName() << "\" (ScriptId=" << scriptId << ")\n";

        std::string err;
        auto result = RunScript(scriptId, &child, &err);
        if (result != ScriptRunResult::Ok)
        {
            std::cerr << "[ServerScriptBridge] Error in Script \""
                      << child.GetName() << "\": " << err << "\n";
        }
    });

    std::cout << "[ServerScriptBridge] Watching Workspace for Script\n";
}

} } } // namespace Sunvoltum::Scripting::Server
