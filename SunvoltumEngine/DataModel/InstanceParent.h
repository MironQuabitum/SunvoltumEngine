#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

#include "../LibSunvoltum.h"

// C4251: STL members (std::function, std::unordered_map) in DLL-exported classes.
// Safe to suppress: clients link the same CRT and use the class only through
// the exported DLL interface, never touching the private fields directly.
#pragma warning(push)
#pragma warning(disable: 4251)

namespace Sunvoltum {

    class Instance;

    // -----------------------------------------------------------------------
    // ChildAddedToken -- RAII subscription handle for ChildAdded event.
    // Destroying the token automatically unsubscribes the callback.
    // Move-only (copy deleted).
    // -----------------------------------------------------------------------
    class LibSunvoltum ChildAddedToken
    {
    public:
        ChildAddedToken() = default;
        ~ChildAddedToken() { Disconnect(); }

        ChildAddedToken(const ChildAddedToken&)            = delete;
        ChildAddedToken& operator=(const ChildAddedToken&) = delete;

        ChildAddedToken(ChildAddedToken&& o) noexcept
            : m_disconnect(std::move(o.m_disconnect)) {}

        ChildAddedToken& operator=(ChildAddedToken&& o) noexcept
        {
            if (this != &o)
            {
                Disconnect();
                m_disconnect = std::move(o.m_disconnect);
            }
            return *this;
        }

        void Disconnect()
        {
            if (m_disconnect) { m_disconnect(); m_disconnect = nullptr; }
        }

        bool IsConnected() const { return m_disconnect != nullptr; }

    private:
        friend class InstanceParent;
        std::function<void()> m_disconnect;
    };

    // -----------------------------------------------------------------------
    // ChildRemovedToken -- RAII subscription handle for ChildRemoved event.
    // -----------------------------------------------------------------------
    class LibSunvoltum ChildRemovedToken
    {
    public:
        ChildRemovedToken() = default;
        ~ChildRemovedToken() { Disconnect(); }

        ChildRemovedToken(const ChildRemovedToken&)            = delete;
        ChildRemovedToken& operator=(const ChildRemovedToken&) = delete;

        ChildRemovedToken(ChildRemovedToken&& o) noexcept
            : m_disconnect(std::move(o.m_disconnect)) {}

        ChildRemovedToken& operator=(ChildRemovedToken&& o) noexcept
        {
            if (this != &o)
            {
                Disconnect();
                m_disconnect = std::move(o.m_disconnect);
            }
            return *this;
        }

        void Disconnect()
        {
            if (m_disconnect) { m_disconnect(); m_disconnect = nullptr; }
        }

        bool IsConnected() const { return m_disconnect != nullptr; }

    private:
        friend class InstanceParent;
        std::function<void()> m_disconnect;
    };

    // -----------------------------------------------------------------------
    // InstanceParent -- hierarchy node interface.
    // Implemented by DataModel (root) and Instance (any object).
    //
    // ChildAdded   fires on every AddInstance call on this node.
    // ChildRemoved fires just before the child is destroyed via RemoveChild.
    // -----------------------------------------------------------------------
    class LibSunvoltum InstanceParent
    {
    public:
        virtual ~InstanceParent() = default;

        virtual Instance& AddInstance(const std::string& name, int8_t classId) = 0;
        virtual Instance* FindByName(const std::string& name) = 0;
        virtual const std::vector<std::unique_ptr<Instance>>& GetChildren() const = 0;

        // Subscribe to ChildAdded. Returns a token -- keep it alive;
        // destroying the token unsubscribes automatically.
        ChildAddedToken SubscribeChildAdded(std::function<void(Instance&)> cb);

        // Subscribe to ChildRemoved. Fires just before the Instance is destroyed.
        ChildRemovedToken SubscribeChildRemoved(std::function<void(Instance&)> cb);

        // Called by AddInstance implementations immediately after object creation.
        void FireChildAdded(Instance& child);

        // Called by RemoveChild implementations before object destruction.
        void FireChildRemoved(Instance& child);

    private:
        using ChildAddedCb   = std::function<void(Instance&)>;
        using ChildRemovedCb = std::function<void(Instance&)>;

        uint64_t                                     m_nextChildCbId      = 1;
        std::unordered_map<uint64_t, ChildAddedCb>   m_childCbs;

        uint64_t                                     m_nextRemovedCbId    = 1;
        std::unordered_map<uint64_t, ChildRemovedCb> m_childRemovedCbs;
    };

} // namespace Sunvoltum

#pragma warning(pop)
