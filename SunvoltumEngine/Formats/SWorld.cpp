#include "SWorld.h"
#include <fstream>
#include <cstring>

#include "../DataModel/DataModel.h"
#include "../DataModel/Instance.h"
#include "../DataModel/PropertyValue.h"

namespace Sunvoltum {
namespace Formats {

    template<typename T>
    static bool ReadRaw(std::istream& s, T& out)
    {
        return static_cast<bool>(s.read(reinterpret_cast<char*>(&out), sizeof(T)));
    }

    template<typename T>
    static bool WriteRaw(std::ostream& s, const T& val)
    {
        return static_cast<bool>(s.write(reinterpret_cast<const char*>(&val), sizeof(T)));
    }

    static bool WriteString(std::ostream& s, const std::string& str)
    {
        auto len = static_cast<uint8_t>(str.size() > 255 ? 255 : str.size());
        if (!WriteRaw(s, len)) return false;
        return static_cast<bool>(s.write(str.c_str(), len));
    }

    static bool ReadString(std::istream& s, std::string& out)
    {
        uint8_t len = 0;
        if (!ReadRaw(s, len)) return false;
        out.resize(len);
        // data() на string до C++17 возвращает const char* — используем &out[0]
        if (len == 0) return true;
        return static_cast<bool>(s.read(&out[0], len));
    }

    // --- Запись одного свойства ---
    static bool WriteProperty(std::ostream& s, PropertyId id, const PropertyEntry& entry)
    {
        if (!WriteRaw(s, id))                                              return false;
        if (!WriteRaw(s, static_cast<uint8_t>(entry.Value.Type)))          return false;
        if (!WriteRaw(s, static_cast<uint8_t>(entry.ReadOnly ? 1 : 0)))    return false;

        const auto& v = entry.Value;
        switch (v.Type)
        {
            case PropertyType::Bool:       return WriteRaw(s, static_cast<uint8_t>(v.Value.AsBool));
            case PropertyType::Int:        return WriteRaw(s, v.Value.AsInt);
            case PropertyType::Float:      return WriteRaw(s, v.Value.AsFloat);
            case PropertyType::Number:     return WriteRaw(s, v.Value.AsNumber);
            case PropertyType::Vector2:    return WriteRaw(s, v.Value.AsVector2);
            case PropertyType::Vector3:    return WriteRaw(s, v.Value.AsVector3);
            case PropertyType::Color3:     return WriteRaw(s, v.Value.AsColor3);
            case PropertyType::CFrame:     return WriteRaw(s, v.Value.AsCFrame);
            case PropertyType::CameraType: return WriteRaw(s, static_cast<uint8_t>(v.Value.AsCameraType));
            case PropertyType::Shape:      return WriteRaw(s, static_cast<uint8_t>(v.Value.AsShape));
        }
        return false;
    }

    // --- Чтение одного свойства ---
    static bool ReadProperty(std::istream& s, Instance& inst)
    {
        PropertyId id      = 0;
        uint8_t    typeRaw = 0;
        uint8_t    roRaw   = 0;

        if (!ReadRaw(s, id))      return false;
        if (!ReadRaw(s, typeRaw)) return false;
        if (!ReadRaw(s, roRaw))   return false;

        PropertyValue val;
        val.Type = static_cast<PropertyType>(typeRaw);
        bool readOnly = roRaw != 0;

        switch (val.Type)
        {
            case PropertyType::Bool:
            {
                uint8_t b = 0; if (!ReadRaw(s, b)) return false;
                val.Value.AsBool = b != 0; break;
            }
            case PropertyType::Int:        if (!ReadRaw(s, val.Value.AsInt))    return false; break;
            case PropertyType::Float:      if (!ReadRaw(s, val.Value.AsFloat))  return false; break;
            case PropertyType::Number:     if (!ReadRaw(s, val.Value.AsNumber)) return false; break;
            case PropertyType::Vector2:    if (!ReadRaw(s, val.Value.AsVector2)) return false; break;
            case PropertyType::Vector3:    if (!ReadRaw(s, val.Value.AsVector3)) return false; break;
            case PropertyType::Color3:     if (!ReadRaw(s, val.Value.AsColor3))  return false; break;
            case PropertyType::CFrame:     if (!ReadRaw(s, val.Value.AsCFrame))  return false; break;
            case PropertyType::CameraType:
            {
                uint8_t ct = 0; if (!ReadRaw(s, ct)) return false;
                val.Value.AsCameraType = static_cast<CameraType>(ct); break;
            }
            case PropertyType::Shape:
            {
                uint8_t sh = 0; if (!ReadRaw(s, sh)) return false;
                val.Value.AsShape = static_cast<Shape>(sh); break;
            }
            default: return false;
        }

        inst.SetProperty(id, val, readOnly);
        return true;
    }

    // -----------------------------------------------------------------------

    bool SWorld::Write(std::ostream& stream, const DataModel& dataModel)
    {
        if (!WriteRaw(stream, MAGIC))   return false;
        if (!WriteRaw(stream, VERSION)) return false;

        const auto& children = dataModel.GetChildren();
        auto count = static_cast<uint32_t>(children.size());
        if (!WriteRaw(stream, count)) return false;

        for (auto& inst : children)
        {
            auto classId = inst->GetClassId();
            if (!WriteRaw(stream, classId))                   return false;
            if (!WriteString(stream, inst->GetName()))         return false;

            // Собираем свойства
            const auto& instRef = *inst;
            // PropertyCount — считаем через перебор 0..255
            // (простое решение без expose всего map наружу)
            std::vector<std::pair<PropertyId, const PropertyEntry*>> props;
            for (uint16_t i = 0; i <= 255; i++)
            {
                auto* entry = instRef.GetPropertyEntry(static_cast<PropertyId>(i));
                if (entry) props.emplace_back(static_cast<PropertyId>(i), entry);
            }

            auto propCount = static_cast<uint8_t>(props.size());
            if (!WriteRaw(stream, propCount)) return false;

            for (size_t pi = 0; pi < props.size(); pi++)
                if (!WriteProperty(stream, props[pi].first, *props[pi].second)) return false;
        }

        return true;
    }

    bool SWorld::Read(std::istream& stream, DataModel& dataModel)
    {
        uint32_t magic = 0;
        if (!ReadRaw(stream, magic) || magic != MAGIC)   return false;

        uint32_t version = 0;
        if (!ReadRaw(stream, version) || version > VERSION) return false;

        uint32_t count = 0;
        if (!ReadRaw(stream, count)) return false;

        for (uint32_t i = 0; i < count; i++)
        {
            int8_t classId = 0;
            if (!ReadRaw(stream, classId)) return false;

            std::string name;
            if (!ReadString(stream, name)) return false;

            Instance& inst = dataModel.AddInstance(name, classId);

            uint8_t propCount = 0;
            if (!ReadRaw(stream, propCount)) return false;

            for (uint8_t p = 0; p < propCount; p++)
                if (!ReadProperty(stream, inst)) return false;
        }

        return true;
    }

    bool SWorld::SaveToFile(const char* path, const DataModel& dataModel)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        return Write(file, dataModel);
    }

    bool SWorld::LoadFromFile(const char* path, DataModel& dataModel)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        return Read(file, dataModel);
    }

} // namespace Formats
} // namespace Sunvoltum
