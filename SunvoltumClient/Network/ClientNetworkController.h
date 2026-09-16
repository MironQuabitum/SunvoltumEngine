#pragma once

#include "Types/EngineMode.h"
#include "Network/NetworkClient.h"
#include "Network/SceneDeserializer.h"
#include "Network/ClientReplicator.h"
#include "DataModel/DataModel.h"
#include <string>
#include <functional>

namespace Sunvoltum {
namespace Client {

    class ClientNetworkController
    {
    public:
        ClientNetworkController() = default;
        ~ClientNetworkController() = default;

        void Init();
        void Connect(const std::string& host, uint16_t port, const std::string& playerName);
        void SetupSceneSynchronization(DataModel& dm, std::function<void()> onSceneLoaded);

        void Poll();
        void Shutdown();
    };

} // namespace Client
} // namespace Sunvoltum
