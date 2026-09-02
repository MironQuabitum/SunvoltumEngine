#include "InstanceRegistry.h"

namespace Sunvoltum {

InstanceRegistry& InstanceRegistry::Get()
{
    static InstanceRegistry s_instance;
    return s_instance;
}

InstanceNetId InstanceRegistry::Assign(Instance& inst)
{
    InstanceNetId id = m_nextId++;
    inst.SetNetId(id);
    m_idToInst[id] = &inst;
    return id;
}

void InstanceRegistry::Register(InstanceNetId id, Instance& inst)
{
    inst.SetNetId(id);
    m_idToInst[id] = &inst;

    // Поддерживаем m_nextId выше максимального известного id
    // чтобы сервер не выдал дублирующий id если реестр используется совместно.
    if (id >= m_nextId)
        m_nextId = id + 1;
}

Instance* InstanceRegistry::Find(InstanceNetId id) const
{
    auto it = m_idToInst.find(id);
    return it != m_idToInst.end() ? it->second : nullptr;
}

void InstanceRegistry::Unregister(InstanceNetId id)
{
    m_idToInst.erase(id);
}

void InstanceRegistry::Clear()
{
    m_idToInst.clear();
    m_nextId = 1;
}

} // namespace Sunvoltum
