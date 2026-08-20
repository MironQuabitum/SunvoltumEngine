#pragma once

#include <istream>
#include <ostream>
#include <cstdint>

#include "../LibSunover.h"
#include "../Types/Mesh.h"

// -----------------------------------------------------------
// Формат .smesh — бинарный меш
//
// Структура файла:
//   [4 bytes] Magic        : "SMSH"
//   [4 bytes] Version      : uint32_t (текущая = 1)
//   [4 bytes] VertexCount  : uint32_t
//   [4 bytes] IndexCount   : uint32_t
//   [VertexCount * sizeof(Vertex)] — данные вершин
//   [IndexCount  * sizeof(uint32_t)] — индексы
//
// Работает с любым std::istream / std::ostream —
// можно читать из файла, из памяти (stringstream), из сети.
// -----------------------------------------------------------

namespace Sunover {
namespace Formats {

    class LibSunover SMesh
    {
    public:
        static constexpr uint32_t MAGIC   = 0x48534D53; // "SMSH"
        static constexpr uint32_t VERSION = 1;

        // Десериализация: читает меш из любого потока.
        // Возвращает пустой Mesh если поток невалиден.
        static Mesh Read(std::istream& stream);

        // Сериализация: записывает меш в любой поток.
        // Возвращает false если запись не удалась.
        static bool Write(std::ostream& stream, const Mesh& mesh);

        // Удобные обёртки для работы с файлами
        static Mesh  LoadFromFile(const char* path);
        static bool  SaveToFile(const char* path, const Mesh& mesh);
    };

} // namespace Formats
} // namespace Sunover
