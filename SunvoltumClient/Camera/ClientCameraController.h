#pragma once

#include "DataModel/DataModel.h"
#include "DataModel/Instance.h"

namespace Sunvoltum {
namespace Client {

    class ClientCameraController
    {
    public:
        ClientCameraController() = default;
        ~ClientCameraController() = default;

        // Создает или настраивает камеру в DataModel
        Instance& SetupCamera(DataModel& dm);

        // Привязывает камеру к Humanoid (или HumanoidRootPart) найденного персонажа
        void AttachToCharacter(Instance& camera, DataModel& dm);
    };

} // namespace Client
} // namespace Sunvoltum
