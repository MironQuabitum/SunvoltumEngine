#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "../LibSunvoltum.h"
#include "PropertyId.h"
#include "PropertyValue.h"

#pragma warning(push)
#pragma warning(disable: 4251) // STL-поля в dll-экспортируемых классах

namespace Sunvoltum {

    class Instance;

    // -------------------------------------------------------------------------
    // PropertyToken — RAII-обёртка для отписки от уведомлений.
    // -------------------------------------------------------------------------
    class LibSunvoltum PropertyToken
    {
    public:
        PropertyToken() = default;

        PropertyToken(const PropertyToken&)            = delete;
        PropertyToken& operator=(const PropertyToken&) = delete;

        PropertyToken(PropertyToken&& other) noexcept;
        PropertyToken& operator=(PropertyToken&& other) noexcept;

        ~PropertyToken();

        void Disconnect();

        bool IsConnected() const { return m_connected; }

    private:
        friend class PropertyManager;

        PropertyToken(Instance* inst, PropertyId pid, uint64_t id,
                      std::shared_ptr<bool> alive);

        Instance*             m_inst      = nullptr;
        PropertyId            m_pid       = 0;
        uint64_t              m_id        = 0;
        bool                  m_connected = false;
        std::shared_ptr<bool> m_managerAlive;
    };

    // -------------------------------------------------------------------------
    // PropertyManager — глобальный менеджер подписок на изменения свойств.
    //
    // Горячий путь (Notify) не использует mutex — движок однопоточный.
    // Subscribe / Unsubscribe / NotifyInstanceDestroyed защищены std::mutex
    // и вызываются только во время Init / Shutdown, не в игровом цикле.
    // -------------------------------------------------------------------------
    class LibSunvoltum PropertyManager
    {
    public:
        static PropertyManager& Get();

        PropertyManager();
        ~PropertyManager();

        PropertyManager(const PropertyManager&)            = delete;
        PropertyManager& operator=(const PropertyManager&) = delete;

        PropertyToken Subscribe(Instance*                                inst,
                                PropertyId                               pid,
                                std::function<void(const PropertyValue&)> callback);

        // Вызывается из Instance::SetProperty (без лока — однопоточный путь).
        void Notify(Instance* inst, PropertyId pid, const PropertyValue& value);

        // Быстрая проверка: если таблица пуста — Notify пропускается полностью.
        bool HasAnySubscribers() const { return !m_table.empty(); }

        // Убрать все подписки на уничтоженный Instance (вызывается из ~Instance).
        void NotifyInstanceDestroyed(Instance* inst);

    private:
        friend class PropertyToken;

        void Unsubscribe(Instance* inst, PropertyId pid, uint64_t id);

        struct Subscription
        {
            uint64_t                                  id;
            std::function<void(const PropertyValue&)> callback;
        };

        std::unordered_map<
            Instance*,
            std::unordered_map<PropertyId, std::vector<Subscription>>
        > m_table;

        std::mutex    m_mutex;
        uint64_t      m_nextId = 1;
        std::shared_ptr<bool> m_alive;
    };

} // namespace Sunvoltum

#pragma warning(pop)
