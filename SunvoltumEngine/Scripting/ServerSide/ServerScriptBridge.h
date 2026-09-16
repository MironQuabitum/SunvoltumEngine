#pragma once

#include <string>
#include <istream>
#include <unordered_map>
#include <vector>
#include <memory>

#include "../../LibSunvoltum.h"
#include "../../DataModel/DataModel.h"
#include "../../DataModel/Instance.h"
#include "../../DataModel/InstanceParent.h"

// Forward-declare lua_State чтобы не тащить Luau в публичный заголовок
struct lua_State;

// Подавляем C4251: STL-контейнеры внутри LibSunvoltum-класса.
// Все поля m_* используются только внутри DLL — клиент к ним не обращается.
#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {
namespace Scripting {
namespace Server {

    // Результат запуска скрипта
    enum class ScriptRunResult
    {
        Ok,             // скрипт выполнен без ошибок (или ушёл в yield)
        CompileError,   // ошибка компиляции Luau bytecode
        RuntimeError,   // ошибка во время выполнения
        NotFound,       // ScriptId не найден в реестре
        Disabled,       // скрипт помечен как Disabled
    };

    // Максимальное число инструкций Luau, исполняемых за один кадр на один корутин.
    // Если скрипт превышает лимит (например, while true do end без wait()),
    // выполнение прерывается через lua_break и возобновляется в следующем кадре.
    static constexpr int kInstructionsPerFrame = 1'000'000;

    // Запись об одном спящем корутине (wait/task.wait)
    struct SleepEntry
    {
        lua_State* thread   = nullptr;  // поток Luau
        int        threadRef = 0;       // lua_ref в registry (защищает от GC)
        double     wakeAt   = 0.0;      // абсолютное время пробуждения (секунды)
    };

    // Запись о корутине, прерванном из-за исчерпания instruction budget.
    // Он будет возобновлён в начале следующего кадра.
    struct BudgetEntry
    {
        lua_State* thread    = nullptr;
        int        threadRef = 0;
    };

    // ServerScriptBridge — Luau VM + реестр скриптов + планировщик задержек.
    //
    // Жизненный цикл:
    //   1. Init(dataModel)           — создаёт lua_State, регистрирует метатаблицы
    //   2. LoadScript(stream/source) — сохраняет исходник, возвращает ScriptId
    //   3. RunScript(id, inst)       — компилирует, регистрирует глобалы и запускает
    //   4. StepScheduler(now)        — вызывать каждый кадр; возобновляет спящие корутины
    //   5. Shutdown()                — закрывает lua_State
    class LibSunvoltum ServerScriptBridge
    {
    public:
        static ServerScriptBridge& Get();

        // Инициализировать VM и передать ссылку на DataModel (нужна для game-глобала).
        // Вызывать ровно один раз до первого RunScript.
        void Init(DataModel* dataModel);

        // Освободить lua_State и очистить реестр.
        void Shutdown();

        bool IsInitialized() const { return m_state != nullptr; }

        // -------------------------------------------------------------------
        //  Реестр исходников
        // -------------------------------------------------------------------

        int LoadScript(std::istream& stream);
        int LoadScriptFromSource(const std::string& source);
        const std::string& GetSource(int scriptId) const;
        bool HasScript(int scriptId) const;
        void UnloadScript(int scriptId);

        // -------------------------------------------------------------------
        //  Выполнение
        // -------------------------------------------------------------------

        // Скомпилировать и запустить скрипт.
        // inst      — инстанс Script-объекта, доступен как `script` в Luau.
        // outError  — сообщение об ошибке (если != nullptr).
        // Если скрипт вызвал wait/task.wait, возвращает Ok (корутин жив в очереди).
        ScriptRunResult RunScript(int scriptId, Instance* scriptInst,
                                  std::string* outError = nullptr);

        // -------------------------------------------------------------------
        //  Планировщик задержек (wait / task.wait)
        // -------------------------------------------------------------------

        // Вызывать каждый кадр передавая текущее абсолютное время в секундах.
        // Возобновляет все корутины, у которых истёк таймер ожидания.
        void StepScheduler(double now);

        // Текущее абсолютное время VM (секунды, обновляется через StepScheduler).
        // Используется C-функцией wait() чтобы вычислить wakeAt.
        double GetCurrentTime() const { return m_currentTime; }

        // Поставить thread в очередь ожидания на duration секунд.
        // Вызывается из C-функции wait() непосредственно перед lua_yield.
        void ScheduleWake(lua_State* thread, int threadRef, double duration);

        // Подписаться на ChildAdded у Workspace — запускать Script автоматически
        // когда инстанс с ненулевым ScriptId добавляется в иерархию Workspace.
        void WatchWorkspace();

    private:
        ServerScriptBridge()  = default;
        ~ServerScriptBridge() = default;

        ServerScriptBridge(const ServerScriptBridge&)            = delete;
        ServerScriptBridge& operator=(const ServerScriptBridge&) = delete;

        void RegisterGlobals(lua_State* L);
        void SetupInterruptCallback();
        void ResumeThread(SleepEntry& entry);
        void ResumeBudgetThread(BudgetEntry& entry);

        // Interrupt callback, вызываемый Luau на каждом safepoint.
        // Устанавливается через lua_callbacks(m_state)->interrupt.
        static void OnInterrupt(lua_State* L, int gc);

        lua_State*  m_state       = nullptr;
        DataModel*  m_dataModel   = nullptr;
        double      m_currentTime = 0.0;   // обновляется в StepScheduler

        // Instruction budget: сбрасывается в начале каждого кадра (StepScheduler).
        // Каждый safepoint декрементирует счётчик; при достижении 0 — lua_break.
        int         m_budgetCounter = kInstructionsPerFrame;

        int                                   m_nextId = 1;
        std::unordered_map<int, std::string>  m_scripts;

        std::vector<SleepEntry>               m_sleeping;
        std::vector<BudgetEntry>              m_interrupted;

        ChildAddedToken                       m_watchToken;

        static const std::string              s_empty;
    };

} // namespace Server
} // namespace Scripting
} // namespace Sunvoltum

#pragma warning(pop)
