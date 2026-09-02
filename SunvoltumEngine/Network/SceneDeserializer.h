#pragma once

// ---------------------------------------------------------------------------
// SceneDeserializer.h — client-side десериализация сцены
//
// Flow:
//   1. OnStartSerialization(r)
//      - Читает totalCount + sessionToken
//      - Сбрасывает внутреннее состояние
//      - Отправляет ReadySerialization(token) — Reliable
//
//   2. OnNewInstance(r)
//      - Читает token, serializeId, parentId, classId, name, свойства
//      - Проверяет token — пакет из чужой сессии отбрасывается
//      - Сохраняет InstanceDesc в m_received[serializeId]
//      - InstanceRef-ы сохраняются как pending для отложенного связывания
//
//   3. OnEndSerialization(r)
//      - Читает token + checksum
//      - Вычисляет локальный checksum из полученных serializeId
//      - Находит дыры: serializeId в [0, totalCount) которых нет в m_received
//      - Если дыры есть → отправляет AskInstance(token, missingIds) — Reliable
//      - Если дыр нет + checksum совпадает → вызывает ApplyTo() + SerializationComplete
//
//   4. OnNewInstance может прийти повторно после AskInstance.
//      После каждого нового пакета повторно вызывается TryFinalize():
//      - Если все объекты получены → ApplyTo() + SerializationComplete
//
//   ApplyTo(DataModel& dm):
//      - BFS по m_received (родители раньше детей, порядок гарантируется BFS на сервере)
//      - Для каждого InstanceDesc: AddInstance в правильного родителя
//      - SetProperty для всех свойств
//      - Второй проход: связывает InstanceRef (pending refs) через построенную
//        таблицу serializeId → Instance*
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "../LibSunvoltum.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceRegistry.h"
#include "../DataModel/PropertyId.h"
#include "../DataModel/PropertyValue.h"
#include "NetworkPacket.h"
#include "NetworkSerializer.h"

namespace Sunvoltum {
namespace Net {

    // -----------------------------------------------------------------------
    // PendingRef — InstanceRef, который нужно разрешить после создания всех объектов
    // -----------------------------------------------------------------------
    struct PendingRef
    {
        Instance*  targetInstance; // объект у которого нужно выставить свойство
        PropertyId propId;         // PropertyId свойства типа InstanceRef
        uint16_t   refSerializeId; // SerializeId объекта на который ссылаемся
    };

    // -----------------------------------------------------------------------
    // InstanceDesc — буфер одного полученного объекта
    // -----------------------------------------------------------------------
    struct InstanceDesc
    {
        uint16_t      serializeId = 0;
        uint16_t      parentId    = SERIALIZE_ID_NONE;
        InstanceNetId netId       = INVALID_INSTANCE_NET_ID; // постоянный id с сервера
        int8_t        classId     = 0;
        std::string   name;

        // Обычные свойства (не InstanceRef)
        std::vector<std::pair<PropertyId, PropertyValue>> properties;

        // InstanceRef — разрешаются отдельным проходом после создания всех объектов
        std::vector<std::pair<PropertyId, uint16_t>> instanceRefs; // propId → serializeId
    };

    // -----------------------------------------------------------------------
    // SceneDeserializer — синглтон
    // -----------------------------------------------------------------------
    class LibSunvoltum SceneDeserializer
    {
    public:
        static SceneDeserializer& Get();

        SceneDeserializer(const SceneDeserializer&)            = delete;
        SceneDeserializer& operator=(const SceneDeserializer&) = delete;

        // Колбэк: сцена полностью применена к DataModel.
        // Вызывается после ApplyTo() — один раз за сессию подключения.
        void SetOnComplete(std::function<void()> cb);

        // Установить целевой DataModel для ApplyTo().
        // Вызывать до старта игрового цикла (до первого Connect).
        void SetDataModel(DataModel* dm);

        // ---------------------------------------------------------------
        // Обработчики входящих пакетов — вызывать из SetOnPacketReceived
        // ---------------------------------------------------------------
        void OnStartSerialization(PacketReader& r);
        void OnNewInstance        (PacketReader& r);
        void OnEndSerialization   (PacketReader& r);

        // Применить все полученные объекты к DataModel.
        // Вызывается автоматически при завершении, но можно вызвать вручную.
        void ApplyTo(DataModel& dm);

        // true если все объекты получены и ApplyTo уже был вызван
        bool IsComplete() const;

        // Список SerializeId которые ещё не получены (для внешней диагностики)
        std::vector<uint16_t> GetMissingIds() const;

    private:
        SceneDeserializer() = default;
        ~SceneDeserializer() = default;

        // Сброс состояния при старте новой сериализации
        void Reset();

        // Проверить: получили ли все объекты? Если да — завершить.
        // dm нужен для ApplyTo; если nullptr — только проверяет.
        void TryFinalize(DataModel* dm);

        // Отправить AskInstance для всех дыр
        void SendAskInstance() const;

        // Отправить SerializationComplete
        void SendComplete() const;

        // ---------------------------------------------------------------
        // Состояние текущей сессии сериализации
        // ---------------------------------------------------------------
        uint32_t m_token      = 0;
        uint16_t m_totalCount = 0;
        uint32_t m_serverChecksum = 0;
        bool     m_endReceived    = false; // получили EndSerialization
        bool     m_complete       = false; // ApplyTo уже вызван

        // serializeId → описание объекта
        std::unordered_map<uint16_t, InstanceDesc> m_received;

        // DataModel для отложенного ApplyTo (сохраняем указатель после OnStartSerialization)
        DataModel* m_dataModel = nullptr;

        std::function<void()> m_onComplete;
    };

} // namespace Net
} // namespace Sunvoltum
