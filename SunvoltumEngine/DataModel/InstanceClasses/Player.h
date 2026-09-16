#pragma once

#include "../PropertyId.h"

namespace Sunvoltum {
namespace Classes {

    constexpr int8_t CLASS_PLAYER = 14;

    // Player — объект игрока, создаётся сервером при подключении.
    // Живёт внутри контейнера Players в DataModel.
    //
    // Свойства:
    //   Username   (String) — отображаемое имя игрока
    //   UserId     (Int)    — уникальный числовой идентификатор аккаунта
    //   NetworkId  (Int)    — идентификатор сетевого соединения (peer id).
    //                         Сервер назначает его при установке соединения.
    //
    // Пример использования (C++):
    //   auto& p = playersInst.AddInstance("Player1", Classes::Player::ClassId);
    //   p.SetProperty(Classes::Player::Username,  PropertyValue::String("Vasya"));
    //   p.SetProperty(Classes::Player::UserId,    PropertyValue::Int(1001));
    //   p.SetProperty(Classes::Player::NetworkId, PropertyValue::Int(peerId));

    struct Player
    {
        static constexpr int8_t ClassId = CLASS_PLAYER;

        // Отображаемое имя игрока (String)
        static constexpr PropertyId Username  = 0;

        // Уникальный идентификатор аккаунта (Int)
        static constexpr PropertyId UserId    = 1;

        // Идентификатор сетевого соединения (Int).
        // Назначается NetworkManager при подключении клиента.
        // Через этот Id сервер знает кому принадлежит Instance
        // с установленным NetworkOwner.
        static constexpr PropertyId NetworkId = 2;

        // Ссылка на модель персонажа (InstanceRef → Model).
        // Сервер устанавливает при спавне, клиент читает для управления камерой.
        // nil пока персонаж не заспавнен или после смерти.
        static constexpr PropertyId Character = 3;
    };

} // namespace Classes
} // namespace Sunvoltum
