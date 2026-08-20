#pragma once

#include <istream>
#include <ostream>
#include <cstdint>

#include "../LibSunover.h"

// -----------------------------------------------------------
// Формат .sworld — бинарный снимок DataModel
//
// Структура файла:
//   [4 bytes] Magic         : "SWLD"
//   [4 bytes] Version       : uint32_t (текущая = 1)
//   [4 bytes] InstanceCount : uint32_t
//
//   Для каждого Instance:
//     [1 byte]  ClassId
//     [1 byte]  NameLength
//     [N bytes] Name (без нуля)
//     [1 byte]  PropertyCount
//
//     Для каждого Property:
//       [1 byte]  PropertyId
//       [1 byte]  PropertyType
//       [1 byte]  ReadOnly
//       [N bytes] Value (размер зависит от PropertyType)
// -----------------------------------------------------------

namespace Sunover {

    class DataModel;

    namespace Formats {

    class LibSunover SWorld
    {
    public:
        static constexpr uint32_t MAGIC   = 0x444C5753; // "SWLD"
        static constexpr uint32_t VERSION = 1;

        // Сохранить DataModel в поток
        static bool Write(std::ostream& stream, const DataModel& dataModel);

        // Загрузить DataModel из потока
        static bool Read(std::istream& stream, DataModel& dataModel);

        // Удобные обёртки для файлов
        static bool SaveToFile(const char* path, const DataModel& dataModel);
        static bool LoadFromFile(const char* path, DataModel& dataModel);
    };

    } // namespace Formats
} // namespace Sunover
