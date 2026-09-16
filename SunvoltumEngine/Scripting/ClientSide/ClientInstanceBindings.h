#pragma once

// ClientInstanceBindings.h
// Зарезервировано для будущих клиентских расширений метатаблицы Instance.

struct lua_State;

namespace Sunvoltum {
namespace Scripting {
namespace Client {

    // Вызывать ПОСЛЕ RegisterSharedBindingsWithDM.
    // Сейчас не добавляет новых методов — placeholder для будущих клиентских биндингов.
    void RegisterClientInstanceBindings(lua_State* L);

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum
