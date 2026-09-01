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
#include "../DataModel/PropertyManager.h"

namespace Sunvoltum {

// ---------------------------------------------------------------------------
// Вспомогательная функция: загрузить OGG-файл в PCM-буфер OpenAL
// ---------------------------------------------------------------------------
static ALuint LoadOgg(const std::string& path)
{
    OggVorbis_File vf;
    if (ov_fopen(path.c_str(), &vf) != 0)
    {
        std::cerr << "[SoundEngine] Не удалось открыть: " << path << std::endl;
        return 0;
    }

    vorbis_info* info = ov_info(&vf, -1);
    ALenum  format     = (info->channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    ALsizei sampleRate = static_cast<ALsizei>(info->rate);

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
                 pcmData.data(), static_cast<ALsizei>(pcmData.size()), sampleRate);

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
// Подписки PropertyManager для одного Sound
// ---------------------------------------------------------------------------
struct SoundSubscriptions
{
    PropertyToken playing;
    PropertyToken volume;
    PropertyToken looped;
    PropertyToken timePosition;
};

// ---------------------------------------------------------------------------
// Запись о зарегистрированном Sound объекте
// ---------------------------------------------------------------------------
struct SoundEntry
{
    Instance* instance = nullptr;
    ALuint    alBuffer = 0;
    ALuint    alSource = 0;
    bool      is3D     = false;
    Instance* parent   = nullptr; // родитель (ShapePart или Workspace)

    // Подписки — живут вместе с Entry, отписываются при разрушении
    SoundSubscriptions tokens;
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

    std::unordered_map<Instance*, SoundEntry> sounds;
    std::unordered_map<std::string, ALuint>   bufferCache;

    Instance* camera = nullptr; // активная камера (слушатель)

    // -----------------------------------------------------------------
    bool InitAL()
    {
        device = alcOpenDevice(nullptr);
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

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

        std::cout << "[SoundEngine] OpenAL инициализирован: "
                  << alcGetString(device, ALC_DEVICE_SPECIFIER) << std::endl;
        return true;
    }

    // -----------------------------------------------------------------
    // Зарегистрировать Sound и подписаться на его свойства
    // -----------------------------------------------------------------
    void RegisterSound(Instance* inst, Instance* parent, bool is3D)
    {
        if (sounds.count(inst)) return;

        std::string soundId = Classes::Sound::GetSoundId(*inst);
        if (soundId.empty()) return;

        ALuint buffer = 0;
        auto it = bufferCache.find(soundId);
        if (it != bufferCache.end())
            buffer = it->second;
        else
        {
            buffer = LoadOgg(soundId);
            bufferCache[soundId] = buffer;
        }
        if (buffer == 0) return;

        ALuint source = 0;
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));

        if (is3D)
        {
            alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
            alSourcef(source, AL_REFERENCE_DISTANCE, 10.0f);
            alSourcef(source, AL_MAX_DISTANCE,       100.0f);
            alSourcef(source, AL_ROLLOFF_FACTOR,     1.0f);
        }
        else
        {
            alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
            alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        }

        SoundEntry entry;
        entry.instance = inst;
        entry.alBuffer = buffer;
        entry.alSource = source;
        entry.is3D     = is3D;
        entry.parent   = parent;

        // --- Подписки PropertyManager ---
        auto& pm = PropertyManager::Get();

        // Playing
        entry.tokens.playing = pm.Subscribe(inst, Classes::Sound::Playing,
            [source](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                if (val.Value.AsBool)
                    alSourcePlay(source);
                else
                    alSourceStop(source);
            });

        // Volume
        entry.tokens.volume = pm.Subscribe(inst, Classes::Sound::Volume,
            [source](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Float) return;
                alSourcef(source, AL_GAIN, val.Value.AsFloat);
            });

        // Looped
        entry.tokens.looped = pm.Subscribe(inst, Classes::Sound::Looped,
            [source](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Bool) return;
                alSourcei(source, AL_LOOPING,
                          val.Value.AsBool ? AL_TRUE : AL_FALSE);
            });

        // TimePosition (перемотка)
        entry.tokens.timePosition = pm.Subscribe(inst, Classes::Sound::TimePosition,
            [source](const PropertyValue& val)
            {
                if (val.Type != PropertyType::Int) return;
                alSourcef(source, AL_SEC_OFFSET,
                          static_cast<float>(val.Value.AsInt));
            });

        // Применяем начальные значения немедленно
        alSourcef(source, AL_GAIN, Classes::Sound::GetVolume(*inst));
        alSourcei(source, AL_LOOPING,
                  Classes::Sound::IsLooped(*inst) ? AL_TRUE : AL_FALSE);
        if (Classes::Sound::IsPlaying(*inst))
            alSourcePlay(source);

        sounds[inst] = std::move(entry);

        std::cout << "[SoundEngine] Зарегистрирован Sound \""
                  << inst->GetName() << "\" ("
                  << (is3D ? "3D" : "ambient")
                  << ") → " << soundId << std::endl;
    }

    // -----------------------------------------------------------------
    // Обход DataModel и регистрация всех Sound
    // -----------------------------------------------------------------
    void ScanDataModel()
    {
        for (const auto& child : engine->DataModel.GetChildren())
        {
            if (child->GetClassId() == Classes::CurrentCamera::ClassId)
                camera = child.get();
        }
        ScanChildren(engine->DataModel.GetChildren(), nullptr);
    }

    void ScanChildren(const std::vector<std::unique_ptr<Instance>>& children,
                      Instance* parentHint)
    {
        for (const auto& uptr : children)
        {
            Instance* inst = uptr.get();
            if (inst->GetClassId() == Classes::Sound::ClassId)
            {
                bool is3D = (parentHint != nullptr &&
                             parentHint->GetClassId() == Classes::ShapePart::ClassId);
                RegisterSound(inst, parentHint, is3D);
            }
            else
            {
                ScanChildren(inst->GetChildren(), inst);
            }
        }
    }

    // -----------------------------------------------------------------
    // Обновление за кадр — только позиционирование
    // Playing/Volume/Looped/TimePosition больше не поллятся: коллбэки.
    // -----------------------------------------------------------------
    void Update(float /*dt*/)
    {
        // --- Позиция слушателя по камере ---
        // Камера меняется каждый кадр (физика/орбита) — поллинг здесь оправдан.
        if (camera)
        {
            const auto* cfProp = camera->GetProperty(Classes::CurrentCamera::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
            {
                const Sunvoltum::CFrame& cf = cfProp->Value.AsCFrame;

                alListener3f(AL_POSITION, cf.Position.X, cf.Position.Y, cf.Position.Z);

                Sunvoltum::Vector3 fwd = cf.Rotation * Sunvoltum::Vector3(0.0f, 0.0f, -1.0f);
                Sunvoltum::Vector3 up  = cf.Rotation * Sunvoltum::Vector3(0.0f, 1.0f,  0.0f);
                float orientation[6] = { fwd.X, fwd.Y, fwd.Z, up.X, up.Y, up.Z };
                alListenerfv(AL_ORIENTATION, orientation);
            }
        }

        // --- Позиции 3D источников ---
        // CFrame родителя (ShapePart) меняется каждый физический тик — поллинг оправдан.
        // Остальные параметры (Playing/Volume/Looped) обновляются только по коллбэку.
        for (auto& [inst, entry] : sounds)
        {
            if (!entry.is3D || !entry.parent) continue;

            const auto* cfProp = entry.parent->GetProperty(Classes::ShapePart::CFrame);
            if (cfProp && cfProp->Type == PropertyType::CFrame)
            {
                const auto& pos = cfProp->Value.AsCFrame.Position;
                alSource3f(entry.alSource, AL_POSITION, pos.X, pos.Y, pos.Z);
            }
        }
    }

    // -----------------------------------------------------------------
    void Cleanup()
    {
        // Токены отпишутся автоматически при разрушении SoundEntry
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

SoundEngine::SoundEngine()  : m_impl(std::make_unique<Impl>()) {}
SoundEngine::~SoundEngine() { if (m_impl && m_impl->initialized) m_impl->Cleanup(); }

bool SoundEngine::Init(Engine& engine)
{
    m_impl->engine = &engine;
    if (!m_impl->InitAL()) return false;
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

bool SoundEngine::IsInitialized() const { return m_impl->initialized; }

} // namespace Sunvoltum
