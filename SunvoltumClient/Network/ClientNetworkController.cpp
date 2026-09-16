#include "ClientNetworkController.h"
#include "Network/NetworkManager.h"
#include <iostream>

namespace Sunvoltum {
namespace Client {

    void ClientNetworkController::Init()
    {
        NetworkManager::Get().Init(EngineMode::Client);

        Net::NetworkClient::Get().SetOnConnected([&](NetworkId myId)
        {
            std::cout << "[ClientNetworkController] Connected to server! NetworkId=" << myId << "\n";
        });

        Net::NetworkClient::Get().SetOnDisconnected([&]()
        {
            std::cout << "[ClientNetworkController] Disconnected from server.\n";
        });

        Net::NetworkClient::Get().SetOnPacketReceived([&](Net::PacketReader& r)
        {
            using PT = Net::PacketType;
            switch (r.Type())
            {
            case PT::StartSerialization:   Net::SceneDeserializer::Get().OnStartSerialization(r); break;
            case PT::NewInstance:          Net::SceneDeserializer::Get().OnNewInstance(r);         break;
            case PT::EndSerialization:     Net::SceneDeserializer::Get().OnEndSerialization(r);    break;
            case PT::InstanceAdded:        Net::ClientReplicator::Get().OnInstanceAdded(r);        break;
            case PT::InstanceRemoved:      Net::ClientReplicator::Get().OnInstanceRemoved(r);      break;
            case PT::PropertyChanged:      Net::ClientReplicator::Get().OnPropertyChanged(r);      break;
            case PT::PropertyUpdate:       Net::ClientReplicator::Get().OnPropertyUpdate(r);       break;
            default: break;
            }
        });
    }

    void ClientNetworkController::Connect(const std::string& host, uint16_t port, const std::string& playerName)
    {
        Net::NetworkClient::Get().Connect(host, port, playerName);
    }

    void ClientNetworkController::SetupSceneSynchronization(DataModel& dm, std::function<void()> onSceneLoaded)
    {
        Net::SceneDeserializer::Get().SetDataModel(&dm);
        Net::SceneDeserializer::Get().SetOnComplete([&dm, onSceneLoaded = std::move(onSceneLoaded)]()
        {
            std::cout << "[ClientNetworkController] Scene fully loaded from server!\n";
            Net::ClientReplicator::Get().SetDataModel(&dm);
            Net::ClientReplicator::Get().PostDeserialize(dm);

            if (onSceneLoaded)
                onSceneLoaded();
        });
    }

    void ClientNetworkController::Poll()
    {
        NetworkManager::Get().Poll();
        Net::NetworkClient::Get().ResendPending();
        Net::NetworkClient::Get().RetryHandshake();
    }

    void ClientNetworkController::Shutdown()
    {
        Net::NetworkClient::Get().Shutdown();
        NetworkManager::Get().Shutdown();
    }

} // namespace Client
} // namespace Sunvoltum
