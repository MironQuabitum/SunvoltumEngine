#pragma once

#include <lua.h>

// forward declare чтобы не тащить DataModel.h в заголовок
namespace Sunvoltum { class DataModel; }

namespace Sunvoltum {
namespace Scripting {
namespace Shared {

    // RegisterSharedBindings — регистрирует метатаблицы и CFrame-глобал.
    // Вызывать сразу после lua_newstate.
    void RegisterSharedBindings(lua_State* L);

    // RegisterSharedBindingsWithDM — регистрирует Instance.new с DataModel.
    // Вызывать после того как DataModel инициализирован и заполнен.
    void RegisterSharedBindingsWithDM(lua_State* L, DataModel* dm);

} // namespace Shared
} // namespace Scripting
} // namespace Sunvoltum
