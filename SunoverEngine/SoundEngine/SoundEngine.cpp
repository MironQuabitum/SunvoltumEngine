#include "SoundEngine.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <vorbis/vorbisfile.h>

#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>

#include "../Core/Engine.h"
#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/InstanceClasses/Sound.h"
#include "../DataModel/InstanceClasses/ShapePart.h"
#include "../DataModel/InstanceClasses/Workspace.h"
#include "../DataModel/InstanceClasses/CurrentCamera.h"

namespace Sunover {

// ---------------------------------------------------------------------------
// Вспомогательная функция: загрузить OGG-файл в PCM-буфер OpenAL
// Возвращает ALuint буфер или 0 при ошибке.
// ---------------------------------------------------------------------------
static ALuint LoadOgg(const std::string& path)
{
    OggVorbis_File vf;

    // ov_fopen ожидает путь в кодировке системы; на Windows используем _wfopen
    // через ov_open_callbacks чтобы поддержать UTF-8 пути в будущем.
    // Пока используем стандартный ov_fopen.
    if (ov_fopen(path.c_str(), &vf) != 0)
    {
        std::cerr << "[SoundEngine] Не удалось открыть: " << path << std::endl;
        return 0;
    }

    vorbis_info* info = ov_info(&vf, -1);
    ALenum format = (info->channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    ALsizei sampleRate = static_cast<ALsizei>(info->rate);

    // Читаем весь файл в PCM
    std::vector<char> pcmData;
    char readBuf[65536];
    int  bitStream = 0;
    long bytesRead  = 0;

    while ((bytesRead = ov_read(&vf, readBuf, sizeof(readBuf), 0, 2, 1, &bitStream)) > 0)
        pcmData.insert(pcmData.end(), readBuf, readBuf + bytesRead);

    ov_clear(&vf);

    if (pcmData.empty())
    {
        std::cerr << "[SoundEngine] Пустой PCM: " << path << std::endl;
        return 0;
    }

    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format,
                 pcmData.data(), static_cast<ALsizei>(pcmData.size()),
                 sampleRate);

    ALenum err = alGetError();
    if (err != AL_NO_ERROR)
    {
        std::cerr << "[SoundEngine] alBufferData error " << err
                  << " для: " << path << std::endl;
        alDeleteBuffers(1, &buffer);
        return 0;
    }

    std::cout << "[SoundEngine] Загружен: " << path
              << " (" << (pcmData.size() / 1024) << " кБ, "
              << info->channels << " ch, " << sampleRate << " Hz)" << std::endl;
    return buffer;
}

// ---------------------------------------------------------------------------
// Запись о зарегистрированном Sound объекте
// ---------------------------------------------------------------------------
struct SoundEntry
{
    Instance* instance  = nullptr; // указатель на Instance в DataModel (не владеющий)
    ALuint    alBuffer  = 0;       // PCM буфер OpenAL
    ALuint    alSource  = 0;       // OpenAL источник
    bool      is3D      = false;   // true = Parent ShapePart, false = ambient
    Instance* parent    = nullptr; // родитель (ShapePart или Workspace)

    // Последнее синхронизированное состояние
    bool    lastPlaying      = false;
    float   lastVolume       = 1.0f;
    bool    lastLooped       = false;
    int32_t lastTimePosition = -1; // -1 = не применялось ещё
};

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct SoundEngine::Impl
{
    ALCdevice*  device  = nullptr;
    ALCcontext* context = nullptr;
    bool        initialized = false;

    Engine* engine = nullptr;

    // Все зарегистрированные звуки. Ключ — указатель на Instance.
    std::unordered_map<Instance*, SoundEntry> sounds;

    // Кэш PCM буферов: путь → ALuint.  Один файл загружается один раз.
    std::unordered_map<std::string, ALuint> bufferCache;

    // Указатель на активную камеру (для позиции слушателя)
    Instance* camera = nullptr;

    // -----------------------------------------------------------------
    // Инициализация OpenAL
    // -----------------------------------------------------------------
    bool InitAL()
    {
        device = alcOpenDevice(nullptr); // nullptr = устройство по умолчанию
        if (!device)
        {
            std::cerr << "[SoundEngine] Не удалось открыть аудиоустройство" << std::endl;
            return false;
        }

        context = alcCreateContext(device, nullptr);
        if (!context || !alcMakeContextCurrent(context))
        {
            std::cerr << "[SoundEngine] Не удалось создать OpenAL контекст" << std::endl;
            if (context) alcDestroyContext(context);
            alcCloseDevice(device);
            device  = nullptr;
            context = nullptr;
            return false;
        }

        // Единицы OpenAL = метры. Studs ~= 0.28 м, но для звука
        // мы оставим 1:1 и настроим RolloffFactor чтобы затухание
        // было приятным при stud-масштабе.
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

        std::cout << "[SoundEngine] OpenAL инициализирован: "
                  << alcGetString(device, ALC_DEVICE_SPECIFIER) << std::endl;
        return true;
    }

    // -----------------------------------------------------------------
    // Зарегистрировать один Sound Instance
    // -----------------------------------------------------------------
    void RegisterSound(Instance* inst, Instance* parent, bool is3D)
    {
        if (sounds.count(inst)) return; // уже зарегистрирован

        std::string soundId = Classes::Sound::GetSoundId(*inst);
        if (soundId.empty()) return;

        // Получить (или загрузить) буфер
        ALuint buffer = 0;
        auto it = bufferCache.find(soundId);
        if (it != bufferCache.end())
        {
            buffer = it->second;
        }
        else
        {
            buffer = LoadOgg(soundId);
            bufferCache[soundId] = buffer;
        }

        if (buffer == 0) return;

        // Создать источник
        ALuint source = 0;
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));

        if (is3D)
        {
            // 3D источник: позиционирование включено
            alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
            alSourcef(source, AL_REFERENCE_DISTANCE, 10.0f);  // studs — полная громкость
            alSourcef(source, AL_MAX_DISTANCE,       100.0f); // studs — затухает до нуля
            alSourcef(source, AL_ROLLOFF_FACTOR,     1.0f);
        }
        else
        {
            // Ambient: источник прикреплён к слушателю, слышно одинаково
            alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
            alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        }

        SoundEntry entry;
        entry.instance = inst;
        entry.alBuffer = buffer;
        entry.alSource = source;
        entry.is3D     = is3D;
        entry.parent   = parent;
        sounds[inst]   = entry;

        std::cout << "[SoundEngine] Зарегистрирован Sound \""
                  << inst->GetName() << "\" ("
                  << (is3D ? "3D" : "ambient")
                  << ") → " << soundId << std::endl;
    }

    // -----------------------------------------------------------------
    // Пройти по DataModel и зарегистрировать все Sound объекты
    // -----------------------------------------------------------------
    void ScanDataModel()
    {
        // Ищем камеру на верхнем уровне
        for (const auto& child : engine->DataModel.GetChildren())
        {
            if (child->GetClassId() == Classes::CurrentCamera::ClassId)
                camera = child.get();
        }

        // Обходим дерево: ищем Sound дочерних Workspace (ambient)
        // и Sound дочерних ShapePart (3D)
        ScanChildren(engine->DataModel.GetChildren(), nullptr);
    }

    void ScanChildren(const std::vector<std::unique_ptr<Instance>>& children, Instance* parentHint)
    {
        for (const auto& uptr : children)
        {
            Instance* inst  = uptr.get();
            int8_t classId  = inst->GetClassId();

            if (classId == Classes::Sound::ClassId)
            {
                // Определяем режим по родителю
                bool is3D = (parentHint != nullptr &&
                             parentHint->GetClassId() == Classes::ShapePart::ClassId);
                RegisterSound(inst, parentHint, is3D);
            }
            else
            {
                // Рекурсивно обходим дочерние
                ScanChildren(inst->GetChildren(), inst);
            }
        }
    }

    // -----------------------------------------------------------------
    // Обновление за кадр
    // -----------------------------------------------------------------
    void Update(float /*dt*/)
    {
        // --- Обновить позицию слушателя по камере ---
        if (camera)
        {
            const auto* cfProp = camera->GetProperty(Classes::CurrentCamera::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
            {
                const Sunover::CFrame& cf = cfProp->Value.AsCFrame;

                // Позиция слушателя
                alListener3f(AL_POSITION,
                             cf.Position.X,
                             cf.Position.Y,
                             cf.Position.Z);

                // Ориентация: forward (-Z в мировых координатах)
                // и up (Y)
                Sunover::Vector3 fwd = cf.Rotation * Sunover::Vector3(0.0f, 0.0f, -1.0f);
                Sunover::Vector3 up  = cf.Rotation * Sunover::Vector3(0.0f, 1.0f,  0.0f);

                float orientation[6] = {
                    fwd.X, fwd.Y, fwd.Z,
                    up.X,  up.Y,  up.Z
                };
                alListenerfv(AL_ORIENTATION, orientation);
            }
        }

        // --- Синхронизировать каждый Sound ---
        for (auto& [inst, entry] : sounds)
        {
            bool    wantPlay     = Classes::Sound::IsPlaying(*inst);
            float   volume       = Classes::Sound::GetVolume(*inst);
            bool    looped       = Classes::Sound::IsLooped(*inst);
            int32_t timePosition = Classes::Sound::GetTimePosition(*inst);

            // Обновить громкость
            if (volume != entry.lastVolume)
            {
                alSourcef(entry.alSource, AL_GAIN, volume);
                entry.lastVolume = volume;
            }

            // Обновить зацикливание
            if (looped != entry.lastLooped)
            {
                alSourcei(entry.alSource, AL_LOOPING, looped ? AL_TRUE : AL_FALSE);
                entry.lastLooped = looped;
            }

            // Перемотка: применяем если значение изменилось
            if (timePosition != entry.lastTimePosition)
            {
                alSourcef(entry.alSource, AL_SEC_OFFSET, static_cast<float>(timePosition));
                entry.lastTimePosition = timePosition;
            }

            // Обновить позицию 3D источника
            if (entry.is3D && entry.parent)
            {
                const auto* cfProp = entry.parent->GetProperty(Classes::ShapePart::CFrame);
                if (cfProp && cfProp->Type == PropertyType::CFrame)
                {
                    const auto& pos = cfProp->Value.AsCFrame.Position;
                    alSource3f(entry.alSource, AL_POSITION, pos.X, pos.Y, pos.Z);
                }
            }

            // Запуск / остановка
            if (wantPlay != entry.lastPlaying)
            {
                if (wantPlay)
                {
                    alSourcePlay(entry.alSource);
                    std::cout << "[SoundEngine] Play: " << inst->GetName() << std::endl;
                }
                else
                {
                    alSourceStop(entry.alSource);
                    std::cout << "[SoundEngine] Stop: " << inst->GetName() << std::endl;
                }
                entry.lastPlaying = wantPlay;
            }
        }
    }

    // -----------------------------------------------------------------
    // Освобождение
    // -----------------------------------------------------------------
    void Cleanup()
    {
        for (auto& [inst, entry] : sounds)
        {
            alSourceStop(entry.alSource);
            alDeleteSources(1, &entry.alSource);
        }
        sounds.clear();

        for (auto& [path, buf] : bufferCache)
            alDeleteBuffers(1, &buf);
        bufferCache.clear();

        if (context)
        {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(context);
            context = nullptr;
        }
        if (device)
        {
            alcCloseDevice(device);
            device = nullptr;
        }

        std::cout << "[SoundEngine] Shutdown" << std::endl;
    }
};

// ---------------------------------------------------------------------------
// SoundEngine — публичные методы
// ---------------------------------------------------------------------------

SoundEngine::SoundEngine()
    : m_impl(std::make_unique<Impl>())
{
}

SoundEngine::~SoundEngine()
{
    if (m_impl && m_impl->initialized)
        m_impl->Cleanup();
}

bool SoundEngine::Init(Engine& engine)
{
    m_impl->engine = &engine;

    if (!m_impl->InitAL())
        return false;

    m_impl->ScanDataModel();
    m_impl->initialized = true;
    return true;
}

void SoundEngine::Tick(float dt)
{
    if (!m_impl->initialized) return;
    m_impl->Update(dt);
}

void SoundEngine::Shutdown()
{
    if (!m_impl->initialized) return;
    m_impl->Cleanup();
    m_impl->initialized = false;
}

bool SoundEngine::IsInitialized() const
{
    return m_impl->initialized;
}

} // namespace Sunover
