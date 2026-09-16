#pragma once

// ServerSide/LuauBindings.h — тонкая обёртка над Shared.
//
// Весь общий код (Instance/DataModel/CFrame биндинги) живёт в Shared/.
// Этот заголовок нужен для обратной совместимости: ServerScriptBridge
// и другой серверный код включают его и получают все Shared-символы.

#include "../Shared/LuauHelpers.h"
#include "../Shared/SharedBindings.h"

namespace Sunvoltum {
namespace Scripting {
namespace Server {

    // Серверные имена метатаблиц — те же что и в Shared (один VM)
    using Shared::MT_INSTANCE;
    using Shared::MT_DATAMODEL;
    using Shared::MT_CFRAME;

    // Серверные алиасы push/check — делегируют в Shared
    using Shared::PushInstance;
    using Shared::CheckInstance;
    using Shared::TestInstance;
    using Shared::PushDataModel;
    using Shared::CheckDataModel;
    using Shared::PushCFrame;
    using Shared::CheckCFrame;
    using Shared::TestCFrame;
    using Shared::PushPropertyValue;
    using Shared::PushVector3Table;
    using Shared::PushColor3Table;
    using Shared::CFrameUD;

    // RegisterSharedBindings — регистрирует все общие метатаблицы и CFrame-глобал.
    using Shared::RegisterSharedBindings;
    using Shared::RegisterSharedBindingsWithDM;

} // namespace Server
} // namespace Scripting
} // namespace Sunvoltum
