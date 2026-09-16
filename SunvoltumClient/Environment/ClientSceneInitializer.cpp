#include "ClientSceneInitializer.h"
#include "DataModel/InstanceClasses/Workspace.h"
#include "DataModel/InstanceClasses/Lighting.h"

namespace Sunvoltum {
namespace Client {

    void ClientSceneInitializer::InitEnvironment(DataModel& dm)
    {
        auto& ws = dm.AddInstance("Workspace", Classes::Workspace::ClassId);
        ws.SetProperty(Classes::Workspace::Gravity,        PropertyValue::Number(196.2));
        ws.SetProperty(Classes::Workspace::PhysicsEnabled, PropertyValue::Bool(true), true);

        auto& lighting = dm.AddInstance("Lighting", Classes::Lighting::ClassId);
        lighting.SetProperty(Classes::Lighting::Brightness,         PropertyValue::Number(2.0));
        lighting.SetProperty(Classes::Lighting::ClockTime,          PropertyValue::Number(14.0));
        lighting.SetProperty(Classes::Lighting::GeographicLatitude, PropertyValue::Number(45.0));
        lighting.SetProperty(Classes::Lighting::UseDefaultSky,      PropertyValue::Bool(true));
    }

} // namespace Client
} // namespace Sunvoltum
