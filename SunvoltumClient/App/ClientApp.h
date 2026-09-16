#pragma once

#include "Core/Engine.h"
#include "Rendering/RenderBridge.h"
#include "Runtime/Runtime.h"
#include "Network/ClientNetworkController.h"
#include "Camera/ClientCameraController.h"

namespace Sunvoltum {
namespace Client {

    class ClientApp
    {
    public:
        ClientApp();
        ~ClientApp();

        int Run();

    private:
        Engine                  m_engine;
        RenderBridge            m_renderBridge;
        Runtime                 m_runtime;
        ClientNetworkController m_networkController;
        ClientCameraController  m_cameraController;
    };

} // namespace Client
} // namespace Sunvoltum
