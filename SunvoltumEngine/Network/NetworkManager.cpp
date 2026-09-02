#include "NetworkManager.h"

#include <uv.h>
#include <cassert>
#include <stdexcept>

namespace Sunvoltum {

    // -------------------------------------------------------
    // Singleton
    // -------------------------------------------------------

    NetworkManager& NetworkManager::Get()
    {
        static NetworkManager instance;
        return instance;
    }

    // -------------------------------------------------------
    // Init
    // -------------------------------------------------------

    void NetworkManager::Init(EngineMode mode)
    {
        if (m_initialized)
            return;

        m_mode = mode;

        if (mode == EngineMode::Standalone)
        {
            // В одиночном режиме libuv не нужен
            m_initialized = true;
            return;
        }

        // Выделяем uv_loop вручную — нельзя хранить полный тип в заголовке
        // (forward-declare uv_loop_s), поэтому используем heap-аллокацию.
        m_loop = new uv_loop_t();
        if (uv_loop_init(m_loop) != 0)
        {
            delete m_loop;
            m_loop = nullptr;
            throw std::runtime_error("NetworkManager: failed to initialize uv_loop");
        }

        m_initialized = true;
    }

    // -------------------------------------------------------
    // Poll — неблокирующий тик (вызывать каждый кадр)
    // -------------------------------------------------------

    void NetworkManager::Poll()
    {
        if (!m_initialized || m_loop == nullptr)
            return;

        // UV_RUN_NOWAIT: обрабатывает все готовые события и возвращается
        // немедленно, не ждёт новых — не блокирует игровой цикл.
        uv_run(m_loop, UV_RUN_NOWAIT);
    }

    // -------------------------------------------------------
    // Shutdown
    // -------------------------------------------------------

    void NetworkManager::Shutdown()
    {
        if (!m_initialized)
            return;

        if (m_loop != nullptr)
        {
            // Закрыть все активные хендлы перед остановкой loop
            uv_walk(m_loop, [](uv_handle_t* handle, void* /*arg*/)
            {
                if (!uv_is_closing(handle))
                    uv_close(handle, nullptr);
            }, nullptr);

            // Дать loop обработать close-колбэки
            uv_run(m_loop, UV_RUN_DEFAULT);

            uv_loop_close(m_loop);
            delete m_loop;
            m_loop = nullptr;
        }

        m_initialized = false;
    }

    // -------------------------------------------------------
    // Getters
    // -------------------------------------------------------

    EngineMode NetworkManager::GetMode() const
    {
        return m_mode;
    }

    uv_loop_s* NetworkManager::GetLoop() const
    {
        return m_loop;
    }

    // -------------------------------------------------------
    // AllocNetworkId — выдаёт следующий свободный NetworkId
    // -------------------------------------------------------

    NetworkId NetworkManager::AllocNetworkId()
    {
        assert(m_mode == EngineMode::Server && "AllocNetworkId вызван не на сервере");
        return m_nextId++;
    }

} // namespace Sunvoltum
