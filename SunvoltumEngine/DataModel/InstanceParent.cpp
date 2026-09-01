#include "InstanceParent.h"

namespace Sunvoltum {

    ChildAddedToken InstanceParent::SubscribeChildAdded(std::function<void(Instance&)> cb)
    {
        uint64_t id = m_nextChildCbId++;
        m_childCbs[id] = std::move(cb);

        ChildAddedToken token;
        token.m_disconnect = [this, id]()
        {
            m_childCbs.erase(id);
        };
        return token;
    }

    void InstanceParent::FireChildAdded(Instance& child)
    {
        // Копируем ключи чтобы коллбэк мог безопасно добавлять новые подписки
        // (хотя на практике это редкость, защита не лишняя).
        for (auto& [id, cb] : m_childCbs)
            cb(child);
    }

} // namespace Sunvoltum
