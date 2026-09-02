#pragma once

// ---------------------------------------------------------------------------
// NetworkSerializer.h — кодирование/декодирование PropertyValue в пакеты
//
// Используется SceneSerializer (сервер) и SceneDeserializer (клиент) для
// записи и чтения произвольных свойств Instance в бинарный протокол.
//
// Формат одного свойства:
//   uint8_t   propertyId
//   uint8_t   propertyType   (PropertyType enum)
//   <encoded value>
//
// Форматы по типу:
//   Bool        → uint8_t  (0 / 1)
//   Int         → int32_t
//   Float       → float
//   Number      → double
//   Vector2     → float x2
//   Vector3     → float x3
//   Color3      → float x3
//   CFrame      → float x3 (Position) + float x9 (Matrix3x3 row-major)
//   CameraType  → uint8_t
//   Shape       → uint8_t
//   String      → uint16_t length + bytes
//   InstanceRef → uint16_t serializeId (0xFFFF = nullptr / не задан)
//
// InstanceRef требует внешней таблицы Instance* → SerializeId;
// передаётся через функциональный параметр instToId.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <functional>

#include "../DataModel/PropertyId.h"
#include "../DataModel/PropertyValue.h"
#include "../DataModel/Instance.h"
#include "NetworkPacket.h"

namespace Sunvoltum {
namespace Net {

    // Sentinel: InstanceRef без маппинга (nullptr или неизвестный объект)
    static constexpr uint16_t SERIALIZE_ID_NONE = 0xFFFF;

    // -----------------------------------------------------------------------
    // WritePropertyValue
    //
    // Записывает в pkt пару (propertyId, encoded value).
    // instToId — функция для преобразования Instance* → SerializeId.
    //            Может возвращать SERIALIZE_ID_NONE если объект не сериализуется.
    // -----------------------------------------------------------------------
    void WritePropertyValue(
        PacketWriter&                              pkt,
        PropertyId                                 propId,
        const PropertyValue&                       value,
        const std::function<uint16_t(Instance*)>&  instToId);

    // -----------------------------------------------------------------------
    // ReadPropertyValue
    //
    // Читает из r пару (propertyId, encoded value).
    // outPropId  — выходной PropertyId
    // outValue   — выходное PropertyValue (для InstanceRef: AsInstanceRef = nullptr,
    //              SerializeId записывается в outRefId для отложенного связывания)
    // outRefId   — SerializeId для InstanceRef (SERIALIZE_ID_NONE если не InstanceRef)
    //
    // Возвращает false при ошибке чтения.
    // -----------------------------------------------------------------------
    bool ReadPropertyValue(
        PacketReader& r,
        PropertyId&   outPropId,
        PropertyValue& outValue,
        uint16_t&      outRefId);

} // namespace Net
} // namespace Sunvoltum
