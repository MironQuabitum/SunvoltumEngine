#include "NetworkSerializer.h"

namespace Sunvoltum {
namespace Net {

// ---------------------------------------------------------------------------
// WritePropertyValue
// ---------------------------------------------------------------------------
void WritePropertyValue(
    PacketWriter&                              pkt,
    PropertyId                                 propId,
    const PropertyValue&                       value,
    const std::function<uint16_t(Instance*)>&  instToId)
{
    pkt.WriteU8(propId);
    pkt.WriteU8(static_cast<uint8_t>(value.Type));

    switch (value.Type)
    {
    case PropertyType::Bool:
        pkt.WriteU8(value.Value.AsBool ? 1u : 0u);
        break;

    case PropertyType::Int:
        pkt.WriteI32(value.Value.AsInt);
        break;

    case PropertyType::Float:
        pkt.WriteF32(value.Value.AsFloat);
        break;

    case PropertyType::Number:
    {
        double d = value.Value.AsNumber;
        uint64_t bits;
        static_assert(sizeof(bits) == sizeof(d), "double size mismatch");
        std::memcpy(&bits, &d, sizeof(bits));
        pkt.WriteU32(static_cast<uint32_t>(bits & 0xFFFFFFFFu));
        pkt.WriteU32(static_cast<uint32_t>(bits >> 32));
        break;
    }

    case PropertyType::Vector2:
        pkt.WriteF32(value.Value.AsVector2.X);
        pkt.WriteF32(value.Value.AsVector2.Y);
        break;

    case PropertyType::Vector3:
        pkt.WriteF32(value.Value.AsVector3.X);
        pkt.WriteF32(value.Value.AsVector3.Y);
        pkt.WriteF32(value.Value.AsVector3.Z);
        break;

    case PropertyType::Color3:
        pkt.WriteF32(value.Value.AsColor3.R);
        pkt.WriteF32(value.Value.AsColor3.G);
        pkt.WriteF32(value.Value.AsColor3.B);
        break;

    case PropertyType::CFrame:
    {
        // Position (3 floats)
        const auto& cf = value.Value.AsCFrame;
        pkt.WriteF32(cf.Position.X);
        pkt.WriteF32(cf.Position.Y);
        pkt.WriteF32(cf.Position.Z);
        // Rotation Matrix3x3, row-major (9 floats)
        pkt.WriteF32(cf.Rotation.R00); pkt.WriteF32(cf.Rotation.R01); pkt.WriteF32(cf.Rotation.R02);
        pkt.WriteF32(cf.Rotation.R10); pkt.WriteF32(cf.Rotation.R11); pkt.WriteF32(cf.Rotation.R12);
        pkt.WriteF32(cf.Rotation.R20); pkt.WriteF32(cf.Rotation.R21); pkt.WriteF32(cf.Rotation.R22);
        break;
    }

    case PropertyType::CameraType:
        pkt.WriteU8(static_cast<uint8_t>(value.Value.AsCameraType));
        break;

    case PropertyType::Shape:
        pkt.WriteU8(static_cast<uint8_t>(value.Value.AsShape));
        break;

    case PropertyType::String:
        pkt.WriteString(value.StringValue);
        break;

    case PropertyType::InstanceRef:
    {
        uint16_t sid = SERIALIZE_ID_NONE;
        if (instToId && value.Value.AsInstanceRef)
            sid = instToId(value.Value.AsInstanceRef);
        pkt.WriteU16(sid);
        break;
    }

    // Если появится новый тип — компилятор предупредит через -Wswitch
    }
}

// ---------------------------------------------------------------------------
// ReadPropertyValue
// ---------------------------------------------------------------------------
bool ReadPropertyValue(
    PacketReader&  r,
    PropertyId&    outPropId,
    PropertyValue& outValue,
    uint16_t&      outRefId)
{
    outRefId = SERIALIZE_ID_NONE;

    uint8_t propId = 0;
    uint8_t typeRaw = 0;
    if (!r.ReadU8(propId)) return false;
    if (!r.ReadU8(typeRaw)) return false;

    outPropId = static_cast<PropertyId>(propId);
    auto type = static_cast<PropertyType>(typeRaw);
    outValue.Type = type;

    switch (type)
    {
    case PropertyType::Bool:
    {
        uint8_t b = 0;
        if (!r.ReadU8(b)) return false;
        outValue.Value.AsBool = (b != 0);
        break;
    }

    case PropertyType::Int:
        if (!r.ReadI32(outValue.Value.AsInt)) return false;
        break;

    case PropertyType::Float:
        if (!r.ReadF32(outValue.Value.AsFloat)) return false;
        break;

    case PropertyType::Number:
    {
        uint32_t lo = 0, hi = 0;
        if (!r.ReadU32(lo)) return false;
        if (!r.ReadU32(hi)) return false;
        uint64_t bits = (static_cast<uint64_t>(hi) << 32) | lo;
        double d;
        std::memcpy(&d, &bits, sizeof(d));
        outValue.Value.AsNumber = d;
        break;
    }

    case PropertyType::Vector2:
        if (!r.ReadF32(outValue.Value.AsVector2.X)) return false;
        if (!r.ReadF32(outValue.Value.AsVector2.Y)) return false;
        break;

    case PropertyType::Vector3:
        if (!r.ReadF32(outValue.Value.AsVector3.X)) return false;
        if (!r.ReadF32(outValue.Value.AsVector3.Y)) return false;
        if (!r.ReadF32(outValue.Value.AsVector3.Z)) return false;
        break;

    case PropertyType::Color3:
        if (!r.ReadF32(outValue.Value.AsColor3.R)) return false;
        if (!r.ReadF32(outValue.Value.AsColor3.G)) return false;
        if (!r.ReadF32(outValue.Value.AsColor3.B)) return false;
        break;

    case PropertyType::CFrame:
    {
        auto& cf = outValue.Value.AsCFrame;
        if (!r.ReadF32(cf.Position.X)) return false;
        if (!r.ReadF32(cf.Position.Y)) return false;
        if (!r.ReadF32(cf.Position.Z)) return false;
        if (!r.ReadF32(cf.Rotation.R00)) return false;
        if (!r.ReadF32(cf.Rotation.R01)) return false;
        if (!r.ReadF32(cf.Rotation.R02)) return false;
        if (!r.ReadF32(cf.Rotation.R10)) return false;
        if (!r.ReadF32(cf.Rotation.R11)) return false;
        if (!r.ReadF32(cf.Rotation.R12)) return false;
        if (!r.ReadF32(cf.Rotation.R20)) return false;
        if (!r.ReadF32(cf.Rotation.R21)) return false;
        if (!r.ReadF32(cf.Rotation.R22)) return false;
        break;
    }

    case PropertyType::CameraType:
    {
        uint8_t v = 0;
        if (!r.ReadU8(v)) return false;
        outValue.Value.AsCameraType = static_cast<CameraType>(v);
        break;
    }

    case PropertyType::Shape:
    {
        uint8_t v = 0;
        if (!r.ReadU8(v)) return false;
        outValue.Value.AsShape = static_cast<Shape>(v);
        break;
    }

    case PropertyType::String:
        if (!r.ReadString(outValue.StringValue)) return false;
        break;

    case PropertyType::InstanceRef:
    {
        uint16_t sid = SERIALIZE_ID_NONE;
        if (!r.ReadU16(sid)) return false;
        outRefId = sid;
        outValue.Value.AsInstanceRef = nullptr; // связывается позже
        break;
    }

    default:
        // Неизвестный тип — прекращаем чтение
        return false;
    }

    return true;
}

} // namespace Net
} // namespace Sunvoltum
