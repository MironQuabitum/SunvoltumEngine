#include "ClientCameraController.h"
#include "DataModel/InstanceClasses/CurrentCamera.h"
#include "Types/CameraType.h"
#include "Types/CFrame.h"
#include <iostream>

namespace Sunvoltum {
namespace Client {

    Instance& ClientCameraController::SetupCamera(DataModel& dm)
    {
        auto& camera = dm.AddInstance("Camera", Classes::CurrentCamera::ClassId);
        camera.SetProperty(Classes::CurrentCamera::FieldOfView,     PropertyValue::Number(70.0));
        camera.SetProperty(Classes::CurrentCamera::CameraMode,      PropertyValue::CameraType(CameraType::Follow));
        camera.SetProperty(Classes::CurrentCamera::MinZoomDistance, PropertyValue::Number(0.0));
        camera.SetProperty(Classes::CurrentCamera::MaxZoomDistance, PropertyValue::Number(50.0));

        // Начальное положение пока сцена не загружена
        camera.SetProperty(Classes::CurrentCamera::CFrame,
            PropertyValue::CFrame(Sunvoltum::CFrame::FromPosition(0.0f, 10.0f, -20.0f)));
        camera.SetProperty(Classes::CurrentCamera::CameraSubject, PropertyValue::Ref(nullptr));

        return camera;
    }

    void ClientCameraController::AttachToCharacter(Instance& camera, DataModel& dm)
    {
        Instance* wsInst = dm.FindByName("Workspace");
        if (!wsInst) return;

        Instance* char1 = wsInst->FindByName("Character1");
        Instance* humanoid = char1 ? char1->FindByName("Humanoid") : nullptr;
        Instance* hrp      = char1 ? char1->FindByName("HumanoidRootPart") : nullptr;

        if (humanoid)
        {
            std::cout << "[ClientCameraController] CameraSubject assigned to Humanoid.\n";
            camera.SetProperty(Classes::CurrentCamera::CameraSubject, PropertyValue::Ref(humanoid));
        }
        else if (hrp)
        {
            std::cout << "[ClientCameraController] CameraSubject assigned to HumanoidRootPart.\n";
            camera.SetProperty(Classes::CurrentCamera::CameraSubject, PropertyValue::Ref(hrp));
        }
    }

} // namespace Client
} // namespace Sunvoltum
