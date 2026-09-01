#pragma once

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include "../../DataModel/Instance.h"
#include "../../DataModel/DataModel.h"
#include "../../DataModel/PropertyValue.h"
#include "../../Types/CFrame.h"

namespace Sunvoltum {
namespace Scripting {
namespace Shared {

    // -----------------------------------------------------------------------
    //  Имена метатаблиц — единые для Server и Client
    // -----------------------------------------------------------------------
    static constexpr const char* MT_INSTANCE  = "Sunvoltum.Instance";
    static constexpr const char* MT_DATAMODEL = "Sunvoltum.DataModel";
    static constexpr const char* MT_CFRAME    = "Sunvoltum.CFrame";

    // -----------------------------------------------------------------------
    //  CFrame userdata — хранит Sunvoltum::CFrame по значению
    // -----------------------------------------------------------------------
    struct CFrameUD
    {
        Sunvoltum::CFrame value;
    };

    // -----------------------------------------------------------------------
    //  Instance
    // -----------------------------------------------------------------------

    // Кладёт Instance* как userdata с метатаблицей MT_INSTANCE.
    // nullptr → nil.
    void PushInstance(lua_State* L, Instance* inst);

    // Проверяет и возвращает Instance* из позиции idx.
    // Бросает Lua-ошибку если тип не совпадает.
    Instance* CheckInstance(lua_State* L, int idx);

    // Возвращает Instance* или nullptr (без Lua-ошибки).
    Instance* TestInstance(lua_State* L, int idx);

    // -----------------------------------------------------------------------
    //  DataModel
    // -----------------------------------------------------------------------
    void PushDataModel(lua_State* L, DataModel* dm);
    DataModel* CheckDataModel(lua_State* L, int idx);

    // -----------------------------------------------------------------------
    //  CFrame userdata
    // -----------------------------------------------------------------------

    // Кладёт копию CFrame как userdata с метатаблицей MT_CFRAME.
    void PushCFrame(lua_State* L, const Sunvoltum::CFrame& cf);

    // Проверяет и возвращает const ref на CFrame внутри userdata.
    // Бросает Lua-ошибку если тип не совпадает.
    const Sunvoltum::CFrame& CheckCFrame(lua_State* L, int idx);

    // Возвращает CFrameUD* или nullptr (без Lua-ошибки).
    CFrameUD* TestCFrame(lua_State* L, int idx);

    // -----------------------------------------------------------------------
    //  PropertyValue → Luau stack
    // -----------------------------------------------------------------------
    void PushPropertyValue(lua_State* L, const PropertyValue& pv);

    // -----------------------------------------------------------------------
    //  Вспомогательные push для простых типов
    // -----------------------------------------------------------------------
    void PushVector3Table(lua_State* L, float x, float y, float z);
    void PushColor3Table (lua_State* L, float r, float g, float b);

} // namespace Shared
} // namespace Scripting
} // namespace Sunvoltum
