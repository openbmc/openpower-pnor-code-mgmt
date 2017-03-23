#pragma once

#include <sdbusplus/server.hpp>
#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

/** @class ItemUpdater
 *  @brief Manages the activation of the version items.
 */
class ItemUpdater
{
    public:
        ItemUpdater() = delete;
        ~ItemUpdater() = default;
        ItemUpdater(const ItemUpdater&) = delete;
        ItemUpdater& operator=(const ItemUpdater&) = delete;
        ItemUpdater(ItemUpdater&&) = delete;
        ItemUpdater& operator=(ItemUpdater&&) = delete;

        /** @brief Constructs ItemUpdater
         *
         * @param[in] bus    - The Dbus bus object
         */
        ItemUpdater(sdbusplus::bus::bus& bus) :
                    busItem(bus),
                    versionMatch(
                            bus,
                           "type='signal',"
                           "member='InterfacesAdded',"
                           "path='/xyz/openbmc_project/software',"
                           "interface='org.freedesktop.DBus.ObjectManager'",
                            createActivation,
                            this)
        {
        }

    private:
        /** @brief Callback function for Software.Version match.
         *  @details Creates an Activation dbus object.
         *
         * @param[in]  msg       - Data associated with subscribed signal
         * @param[in]  userData  - Pointer to this object instance
         * @param[out] retError  - Required param
         */
        static int createActivation(sd_bus_message* msg,
                                    void* userData,
                                    sd_bus_error* retError);

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& busItem;

        /** @brief Persistent map of Activation dbus objects and their
          * version id */
        std::map<uint32_t, std::unique_ptr<Activation>> activations;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::server::match::match versionMatch;
};

} // namespace manager
} // namespace software
} // namespace openpower

