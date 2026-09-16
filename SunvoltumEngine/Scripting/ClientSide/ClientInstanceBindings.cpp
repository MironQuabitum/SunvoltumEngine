// ClientInstanceBindings.cpp
// Клиентские расширения метатаблицы Instance.
// SharedBindings покрывает все свойства Instance включая Motor6D.
// Здесь регистрируются будущие клиент-специфичные методы (NetworkOwner и т.п.).

#include "ClientInstanceBindings.h"

#include <iostream>

namespace Sunvoltum {
namespace Scripting {
namespace Client {

void RegisterClientInstanceBindings(lua_State* /*L*/)
{
    // Placeholder — будущие клиентские методы Instance регистрируются здесь.
    std::cout << "[ClientInstanceBindings] Registered (no extensions yet)\n";
}

} // namespace Client
} // namespace Scripting
} // namespace Sunvoltum
