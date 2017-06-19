#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include "xyz/openbmc_project/Common/FilePath/server.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Common::server::FilePath>;

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  DBus API.
 */
class Version : public VersionInherit
{
    public:
        /** @brief Constructs Version Software Manager.
         *
         * @param[in] bus            - The Dbus bus object
         * @param[in] objPath        - The Dbus object path
         * @param[in] versionId      - The version identifier
         * @param[in] versionPurpose - The version purpose
         * @param[in] filePath       - The image filesystem path
         */
        Version(sdbusplus::bus::bus& bus,
                const std::string& objPath,
                const std::string& versionId,
                VersionPurpose versionPurpose,
                const std::string& filePath) : VersionInherit(
                    bus, (objPath).c_str(), true)
        {
            // Set properties.
            purpose(versionPurpose);
            version(versionId);
            path(filePath);

            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Read the manifest file to get the value of the key.
         *
         * @param[in] filePath - The path to file which contains the value
         *                       of keys.
         * @param[in] keys     - A map of keys with empty values.
         *
         * @return The map of keys with filled values.
         **/
        static std::map<std::string, std::string> getValue(
                const std::string& filePath,
                std::map<std::string, std::string> keys);

        /**
         * @brief Get the Version id.
         *
         * @param[in] version     - The image version.
         *
         * @return The id.
         */
        static std::string getId(const std::string& version);
};

} // namespace updater
} // namespace software
} // namespace openpower
