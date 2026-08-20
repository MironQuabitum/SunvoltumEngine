#include "SMesh.h"
#include <fstream>
#include <cstring>

namespace Sunover {
namespace Formats {

    // Хелперы для бинарного чтения/записи
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

    Mesh SMesh::Read(std::istream& stream)
    {
        // Проверяем magic
        uint32_t magic = 0;
        if (!ReadRaw(stream, magic) || magic != MAGIC)
            return {};

        // Версия
        uint32_t version = 0;
        if (!ReadRaw(stream, version) || version != VERSION)
            return {};

        uint32_t vertexCount = 0;
        uint32_t indexCount  = 0;
        if (!ReadRaw(stream, vertexCount)) return {};
        if (!ReadRaw(stream, indexCount))  return {};

        Mesh mesh;
        mesh.Vertices.resize(vertexCount);
        mesh.Indices.resize(indexCount);

        // Читаем вершины как сырые байты (Vertex — POD-совместимая структура)
        if (!stream.read(reinterpret_cast<char*>(mesh.Vertices.data()),
                         vertexCount * sizeof(Vertex)))
            return {};

        if (!stream.read(reinterpret_cast<char*>(mesh.Indices.data()),
                         indexCount * sizeof(uint32_t)))
            return {};

        return mesh;
    }

    bool SMesh::Write(std::ostream& stream, const Mesh& mesh)
    {
        if (!WriteRaw(stream, MAGIC))   return false;
        if (!WriteRaw(stream, VERSION)) return false;

        auto vertexCount = static_cast<uint32_t>(mesh.Vertices.size());
        auto indexCount  = static_cast<uint32_t>(mesh.Indices.size());
        if (!WriteRaw(stream, vertexCount)) return false;
        if (!WriteRaw(stream, indexCount))  return false;

        if (!stream.write(reinterpret_cast<const char*>(mesh.Vertices.data()),
                          vertexCount * sizeof(Vertex)))
            return false;

        if (!stream.write(reinterpret_cast<const char*>(mesh.Indices.data()),
                          indexCount * sizeof(uint32_t)))
            return false;

        return true;
    }

    Mesh SMesh::LoadFromFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return {};
        return Read(file);
    }

    bool SMesh::SaveToFile(const char* path, const Mesh& mesh)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        return Write(file, mesh);
    }

} // namespace Formats
} // namespace Sunover
