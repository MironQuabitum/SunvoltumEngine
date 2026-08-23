#pragma once

#include "../PropertyId.h"
#include "../Instance.h"
#include "../PropertyValue.h"

namespace Sunover {
namespace Classes {

    constexpr int8_t CLASS_SOUND = 8;

    struct Sound
    {
        static constexpr int8_t ClassId = CLASS_SOUND;

        // Путь к аудиофайлу относительно PlatformContent (например "sounds/oof.ogg")
        static constexpr PropertyId SoundId   = 0; // String

        // Воспроизводится ли звук в данный момент.
        // Установи в true чтобы запустить, в false чтобы остановить.
        // SoundEngine обновляет это значение по факту воспроизведения.
        static constexpr PropertyId Playing   = 1; // Bool

        // Громкость: 0.0 = тишина, 1.0 = полная громкость
        static constexpr PropertyId Volume    = 2; // Float

        // Зациклить воспроизведение
        static constexpr PropertyId Looped        = 3; // Bool

        // Позиция воспроизведения в секундах (перемотка).
        // Установи нужное значение до или во время воспроизведения.
        // SoundEngine применит его через AL_SEC_OFFSET.
        static constexpr PropertyId TimePosition  = 4; // Int

        // Инициализировать свойства Sound значениями по умолчанию.
        // Вызывается сразу после AddInstance.
        static void Init(Instance& inst)
        {
            inst.SetProperty(SoundId,       PropertyValue::String(""));
            inst.SetProperty(Playing,       PropertyValue::Bool(false));
            inst.SetProperty(Volume,        PropertyValue::Float(1.0f));
            inst.SetProperty(Looped,        PropertyValue::Bool(false));
            inst.SetProperty(TimePosition,  PropertyValue::Int(0));
        }

        // Геттеры — возвращают значение свойства или дефолт если свойство не задано

        static std::string GetSoundId(const Instance& inst)
        {
            const auto* p = inst.GetProperty(SoundId);
            return (p && p->Type == PropertyType::String) ? p->StringValue : "";
        }

        static bool IsPlaying(const Instance& inst)
        {
            const auto* p = inst.GetProperty(Playing);
            return (p && p->Type == PropertyType::Bool) ? p->Value.AsBool : false;
        }

        static float GetVolume(const Instance& inst)
        {
            const auto* p = inst.GetProperty(Volume);
            return (p && p->Type == PropertyType::Float) ? p->Value.AsFloat : 1.0f;
        }

        static bool IsLooped(const Instance& inst)
        {
            const auto* p = inst.GetProperty(Looped);
            return (p && p->Type == PropertyType::Bool) ? p->Value.AsBool : false;
        }

        static int32_t GetTimePosition(const Instance& inst)
        {
            const auto* p = inst.GetProperty(TimePosition);
            return (p && p->Type == PropertyType::Int) ? p->Value.AsInt : 0;
        }
    };

} // namespace Classes
} // namespace Sunover
