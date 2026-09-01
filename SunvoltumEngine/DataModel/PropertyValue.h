#pragma once

#include <cstdint>
#include <string>
#include "../Types/Color3.h"
#include "../Types/Vector2.h"
#include "../Types/Vector3.h"
#include "../Types/CFrame.h"
#include "../Types/Number.h"
#include "../Types/CameraType.h"
#include "../Types/Shape.h"

namespace Sunvoltum {

    class Instance; // forward declaration — Instance* хранится в InstanceRef

    enum class PropertyType : uint8_t
    {
        Bool,
        Int,
        Float,
        Number,
        Vector2,
        Vector3,
        Color3,
        CFrame,
        CameraType,
        Shape,
        String,       // std::string — хранится в поле StringValue, а не в union
        InstanceRef,  // Instance* — слабый указатель (не владеет)
    };

    struct PropertyValue
    {
        PropertyType Type = PropertyType::Int;

        union Data
        {
            bool                 AsBool;
            int32_t              AsInt;
            float                AsFloat;
            Sunvoltum::Number      AsNumber;
            Sunvoltum::Vector2     AsVector2;
            Sunvoltum::Vector3     AsVector3;
            Sunvoltum::Color3      AsColor3;
            Sunvoltum::CFrame      AsCFrame;
            Sunvoltum::CameraType  AsCameraType;
            Sunvoltum::Shape       AsShape;
            Instance*            AsInstanceRef; // слабый указатель, не владеет

            Data() : AsInt(0) {}
            ~Data() {}
        } Value;

        // Строковое значение — хранится отдельно от union
        // (std::string нельзя поместить в union без placement new/destroy)
        std::string StringValue;

        PropertyValue() = default;

        static PropertyValue Bool(bool v)
        {
            PropertyValue p; p.Type = PropertyType::Bool; p.Value.AsBool = v; return p;
        }
        static PropertyValue Int(int32_t v)
        {
            PropertyValue p; p.Type = PropertyType::Int; p.Value.AsInt = v; return p;
        }
        static PropertyValue Float(float v)
        {
            PropertyValue p; p.Type = PropertyType::Float; p.Value.AsFloat = v; return p;
        }
        static PropertyValue Number(Sunvoltum::Number v)
        {
            PropertyValue p; p.Type = PropertyType::Number; p.Value.AsNumber = v; return p;
        }
        static PropertyValue Vector2(const Sunvoltum::Vector2& v)
        {
            PropertyValue p; p.Type = PropertyType::Vector2; p.Value.AsVector2 = v; return p;
        }
        static PropertyValue Vector3(const Sunvoltum::Vector3& v)
        {
            PropertyValue p; p.Type = PropertyType::Vector3; p.Value.AsVector3 = v; return p;
        }
        static PropertyValue Color3(const Sunvoltum::Color3& v)
        {
            PropertyValue p; p.Type = PropertyType::Color3; p.Value.AsColor3 = v; return p;
        }
        static PropertyValue CFrame(const Sunvoltum::CFrame& v)
        {
            PropertyValue p; p.Type = PropertyType::CFrame; p.Value.AsCFrame = v; return p;
        }
        static PropertyValue CameraType(Sunvoltum::CameraType v)
        {
            PropertyValue p; p.Type = PropertyType::CameraType; p.Value.AsCameraType = v; return p;
        }
        static PropertyValue Shape(Sunvoltum::Shape v)
        {
            PropertyValue p; p.Type = PropertyType::Shape; p.Value.AsShape = v; return p;
        }
        static PropertyValue String(const std::string& v)
        {
            PropertyValue p; p.Type = PropertyType::String; p.StringValue = v; return p;
        }
        static PropertyValue Ref(Instance* v)
        {
            PropertyValue p; p.Type = PropertyType::InstanceRef; p.Value.AsInstanceRef = v; return p;
        }
    };

} // namespace Sunvoltum
