#pragma once

#include <string>
#include <istream>
#include <unordered_map>
#include <vector>

#include "../../LibSunvoltum.h"
#include "../../DataModel/DataModel.h"
#include "../../DataModel/Instance.h"
#include "../../DataModel/InstanceParent.h"
#include "../../Runtime/IInputSource.h"

struct lua_State;

#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {
namespace Scripting {
namespace Client {

    // Результат запуска LocalScript
    enum class LocalScriptRunResult
    {
        Ok,
        CompileError,
        RuntimeError,
        NotFound,
    };

    // Запись об одном спящем корутине (wait / task.wait)
    struct ClientSleepEntry
    {
        lua_State* thread    = nullptr;
        int        threadRef = 0;
        double     wakeAt    = 0.0;
    };

    struct ClientBudgetEntry
    {
        lua_State* thread    = nullptr;
        int        threadRef = 0;
    };

    static constexpr int kClientInstructionsPerFrame = 1'000'000;

    // ClientScriptBridge — полноценный Luau VM для LocalScript на клиенте.
    //
    // Жизненный цикл:
    //   1. Init(dataModel)              — создаёт lua_State, регистрирует биндинги
    //   2. SetInputSource(input)        — подключает IInputSource для UserInputService
    //   3. LoadScriptFromSource(source) — сохраняет исходник, возвращает id
    //   4. RunScript(id, inst)          — компилирует и запускает LocalScript
    //   5. FireRenderStepped(dt)        — вызывать каждый рендер-кадр
    //   6. FireHeartbeat(dt)            — вызывать каждый тик
    //   7. StepScheduler(now)           — вызывать каждый тик (resumit wait())
    //   8. Shutdown()                   — закрывает lua_State
    class LibSunvoltum ClientScriptBridge
    {
    public:
        static ClientScriptBridge& Get();

        void Init(DataModel* dataModel);
        void Shutdown();
        bool IsInitialized() const { return m_state != nullptr; }

        // Источник ввода — можно установить после Init, до первого RunScript
        void SetInputSource(IInputSource* input);

        // -------------------------------------------------------------------
        //  Реестр исходников
        // -------------------------------------------------------------------
        int  LoadScript(std::istream& stream);
        int  LoadScriptFromSource(const std::string& source);
        bool HasScript(int scriptId) const;

        // -------------------------------------------------------------------
        //  Выполнение
        // -------------------------------------------------------------------
        LocalScriptRunResult RunScript(int scriptId, Instance* scriptInst,
                                       std::string* outError = nullptr);

        // -------------------------------------------------------------------
        //  Кадровые обновления — вызывать из Runtime лямбд
        // -------------------------------------------------------------------

        // Опрос ввода + InputBegan/Ended + все RunService.RenderStepped коннекты
        void FireRenderStepped(double dt);

        // Все RunService.Heartbeat коннекты
        void FireHeartbeat(double dt);

        // Scheduler для wait() / task.wait()
        void StepScheduler(double now);

        double GetCurrentTime() const { return m_currentTime; }
        void   ScheduleWake(lua_State* thread, int threadRef, double duration);

        // Подписаться на ChildAdded у Workspace — запускать LocalScript автоматически
        // когда инстанс с ненулевым ScriptId добавляется в иерархию Workspace.
        void WatchWorkspace();

    private:
        ClientScriptBridge()  = default;
        ~ClientScriptBridge() = default;

        ClientScriptBridge(const ClientScriptBridge&)            = delete;
        ClientScriptBridge& operator=(const ClientScriptBridge&) = delete;

        void RegisterGlobals(lua_State* L);
        void SetupInterruptCallback();
        void ResumeThread(ClientSleepEntry& entry);
        void ResumeBudgetThread(ClientBudgetEntry& entry);

        static void OnInterrupt(lua_State* L, int gc);

        lua_State*    m_state       = nullptr;
        DataModel*    m_dataModel   = nullptr;
        IInputSource* m_input       = nullptr;
        double        m_currentTime = 0.0;
        int           m_budgetCounter = kClientInstructionsPerFrame;

        int                                   m_nextId = 1;
        std::unordered_map<int, std::string>  m_scripts;
        std::vector<ClientSleepEntry>         m_sleeping;
        std::vector<ClientBudgetEntry>        m_interrupted;

        // RAII-токен подписки на ChildAdded у Workspace
        ChildAddedToken                       m_watchToken;

        static const std::string s_empty;
    };

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum

#pragma warning(pop)
