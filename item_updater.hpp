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

        /**
         * @brief process the tarball provided by validating its
         *        contents, performing untar and then unsquashing
         *        the pnor squashfs image
         *
         * @param[in]  tarballFilePath  - Path to the tarball.
         * @param[in]  versionId        - The software version ID.
         * @param[out] result    - 0 --> if Image Processing Passed.
         *                       - -1--> if Failed.
         */
        static int processImage(const std::string& tarballFilePath,
                                std::string& versionId);

        /**
         * @brief Creates a folder in the default location where
         *        all images will be stored.
         *
         * @param[in] versionId        - The software version ID.
         * @param[out] result    - 0 --> if Untar Passed.
         *                       - -1--> if Failed.
         */
        static int createFolder(std::string& imageFolder);

        /**
         * @brief Untar the tarball to extract MANIFEST and squashfs image
         *
         * @param[in]  tarballFilePath  - Path to the tarball.
         * @param[in]  versionId        - The software version ID.
         * @param[out] result    - 0 --> if Untar Passed.
         *                       - -1--> if Failed.
         */
        static int unTAR(const std::string& tarballFilePath,
                         std::string& versionId);

        /**
         * @brief Validates the given tarball by checking files.
         *
         * @param[in]  tarballFilePath  - Path to the tarball.
         * @param[out] result    - 0 --> if validation was successful
         *                       - -1--> Otherwise
         */
        static int validateTarball(const std::string& tarballFilePath);

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& busItem;

        /** @brief Persistent map of Activation dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Activation>> activations;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::server::match::match versionMatch;
};

} // namespace manager
} // namespace software
} // namespace openpower

