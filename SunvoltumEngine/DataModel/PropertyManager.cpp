#include "PropertyManager.h"
#include "Instance.h"

#include <algorithm>

namespace Sunvoltum {

    // =========================================================================
    // PropertyManager
    // =========================================================================

    PropertyManager& PropertyManager::Get()
    {
        // Статический экземпляр — живёт всё время работы программы
        static PropertyManager s_instance;
        return s_instance;
    }

    PropertyManager::PropertyManager()
        : m_alive(std::make_shared<bool>(true))
    {
    }

    PropertyManager::~PropertyManager()
    {
        // Сигнализируем всем живым токенам что менеджер умер
        *m_alive = false;
    }

    // -------------------------------------------------------------------------
    // Subscribe
    // -------------------------------------------------------------------------

    PropertyToken PropertyManager::Subscribe(
        Instance*                                inst,
        PropertyId                               pid,
        std::function<void(const PropertyValue&)> callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        uint64_t id = m_nextId++;
        m_table[inst][pid].push_back({ id, std::move(callback) });

        return PropertyToken(inst, pid, id, m_alive);
    }

    // -------------------------------------------------------------------------
    // Notify — оптимизированный горячий путь.
    // Движок однопоточный: mutex нужен только для Subscribe/Unsubscribe.
    // В Notify мы читаем таблицу без лока — Subscribe/Unsubscribe происходят
    // только во время Init, не во время игрового цикла.
    // -------------------------------------------------------------------------
    void PropertyManager::Notify(Instance* inst, PropertyId pid, const PropertyValue& value)
    {
        // Быстрый выход если вообще нет подписчиков ни на что
        if (m_table.empty()) return;

        auto instIt = m_table.find(inst);
        if (instIt == m_table.end()) return;

        auto pidIt = instIt->second.find(pid);
        if (pidIt == instIt->second.end()) return;

        // Вызываем коллбэки напрямую без копирования вектора и без лока.
        // Если коллбэк вызывает SetProperty → Notify → снова входим сюда —
        // это допустимо т.к. итерация по вектору коллбэков завершится до
        // возможного изменения этого же вектора (Subscribe/Unsubscribe из
        // коллбэка во время игрового цикла не происходит).
        for (const auto& sub : pidIt->second)
            sub.callback(value);
    }

    // -------------------------------------------------------------------------
    // NotifyInstanceDestroyed
    // -------------------------------------------------------------------------

    void PropertyManager::NotifyInstanceDestroyed(Instance* inst)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_table.erase(inst);
    }

    // -------------------------------------------------------------------------
    // Unsubscribe (private, вызывается из PropertyToken)
    // -------------------------------------------------------------------------

    void PropertyManager::Unsubscribe(Instance* inst, PropertyId pid, uint64_t id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto instIt = m_table.find(inst);
        if (instIt == m_table.end()) return;

        auto pidIt = instIt->second.find(pid);
        if (pidIt == instIt->second.end()) return;

        auto& subs = pidIt->second;
        subs.erase(std::remove_if(subs.begin(), subs.end(),
            [id](const Subscription& s) { return s.id == id; }),
            subs.end());

        // Чистим пустые контейнеры чтобы не накапливать мусор
        if (subs.empty())
            instIt->second.erase(pidIt);

        if (instIt->second.empty())
            m_table.erase(instIt);
    }

    // =========================================================================
    // PropertyToken
    // =========================================================================

    PropertyToken::PropertyToken(Instance* inst, PropertyId pid, uint64_t id,
                                 std::shared_ptr<bool> alive)
        : m_inst(inst)
        , m_pid(pid)
        , m_id(id)
        , m_connected(true)
        , m_managerAlive(std::move(alive))
    {
    }

    PropertyToken::PropertyToken(PropertyToken&& other) noexcept
        : m_inst(other.m_inst)
        , m_pid(other.m_pid)
        , m_id(other.m_id)
        , m_connected(other.m_connected)
        , m_managerAlive(std::move(other.m_managerAlive))
    {
        other.m_connected = false;
    }

    PropertyToken& PropertyToken::operator=(PropertyToken&& other) noexcept
    {
        if (this != &other)
        {
            Disconnect();

            m_inst          = other.m_inst;
            m_pid           = other.m_pid;
            m_id            = other.m_id;
            m_connected     = other.m_connected;
            m_managerAlive  = std::move(other.m_managerAlive);

            other.m_connected = false;
        }
        return *this;
    }

    PropertyToken::~PropertyToken()
    {
        Disconnect();
    }

    void PropertyToken::Disconnect()
    {
        if (!m_connected) return;

        // Проверяем что менеджер ещё жив
        if (m_managerAlive && *m_managerAlive)
            PropertyManager::Get().Unsubscribe(m_inst, m_pid, m_id);

        m_connected = false;
    }

} // namespace Sunvoltum
