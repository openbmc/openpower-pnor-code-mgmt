#pragma once

#include <sdbusplus/server.hpp>
#include "activation.hpp"
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>

namespace openpower
{
namespace software
{
namespace updater
{

using ItemUpdaterInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset>;

/** @class ItemUpdater
 *  @brief Manages the activation of the version items.
 */
class ItemUpdater : public ItemUpdaterInherit
{
    public:
        /** @brief Constructs ItemUpdater
         *
         * @param[in] bus    - The Dbus bus object
         */
        ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
                    ItemUpdaterInherit(bus, path.c_str()),
                    bus(bus),
                    versionMatch(
                            bus,
                           "type='signal',"
                           "member='InterfacesAdded',"
                           "path='/xyz/openbmc_project/software',"
                           "interface='org.freedesktop.DBus.ObjectManager'",
                            createActivation,
                            this)
        {
fprintf(stderr, "constructor()\n");
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

        /**
         * @brief Get the extended version from the specified file.
         *
         * @param[in] manifestFilePath  - File to read.
         *
         * @return The extended version.
         */
        static std::string getExtendedVersion(const std::string&
                                               manifestFilePath);
        /**
         * @brief Validates the presence of SquashFS iamge in the image dir.
         *
         * @param[in]  versionId - The software version ID.
         * @param[out] result    - 0 --> if validation was successful
         *                       - -1--> Otherwise
         */
        static int validateSquashFSImage(const std::string& versionId);

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& bus;

        /** @brief Persistent map of Activation dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Activation>> activations;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::server::match::match versionMatch;

        void reset() override;
};

} // namespace updater
} // namespace software
} // namespace openpower
