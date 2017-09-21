#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"
#include "xyz/openbmc_project/Object/Delete/server.hpp"
#include "xyz/openbmc_project/Common/FilePath/server.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

class ItemUpdater;

using VersionInherit = sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Software::server::Version,
        sdbusplus::xyz::openbmc_project::Object::server::Delete,
        sdbusplus::xyz::openbmc_project::Common::server::FilePath>;

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  D-Bus API.
 */
class Version : public VersionInherit
{
    public:
        /** @brief Constructs Version Software Manager.
         *
         * @param[in] bus            - The D-Bus bus object
         * @param[in] objPath        - The D-Bus object path
         * @param[in] versionString  - The version string
         * @param[in] versionPurpose - The version purpose
         * @param[in] filePath       - The image filesystem path
         * @param[in] parent         - The version's parent
         */
        Version(sdbusplus::bus::bus& bus,
                const std::string& objPath,
                const std::string& versionString,
                VersionPurpose versionPurpose,
                const std::string& filePath,
                ItemUpdater& parent) :
                        VersionInherit(bus, (objPath).c_str(), true),
                        parent(parent)
        {
            // Set properties.
            purpose(versionPurpose);
            version(versionString);
            path(filePath);

            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Read the manifest file to get the value of the key.
         *
         * @param[in] filePath - The path to the file which contains the value
         *                       of keys.
         * @param[in] keys     - A map of keys with empty values.
         *
         * @return The map of keys with filled values.
         **/
        static std::map<std::string, std::string> getValue(
                const std::string& filePath,
                std::map<std::string, std::string> keys);

        /**
         * @brief Calculate the version id from the version string.
         *
         * @details The version id is a unique 8 hexadecimal digit id
         *          calculated from the version string.
         *
         * @param[in] version - The image version string (e.g. v1.99.10-19).
         *
         * @return The id.
         */
        static std::string getId(const std::string& version);

        /** @brief Deletes the D-Bus object and removes image.
         *
         */
        void delete_() override;

    private:
        ItemUpdater& parent;

};

} // namespace updater
} // namespace software
} // namespace openpower
