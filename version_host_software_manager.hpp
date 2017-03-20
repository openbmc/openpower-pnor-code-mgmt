#pragma once

#include <sdbusplus/bus.hpp>
#include "xyz/openbmc_project/Software/Version/server.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version>;

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  DBus API.
 */
class Version : public VersionInherit
{
    public:
        /** @brief Constructs Version Software Manager
         *
         * @param[in] bus       - The Dbus bus object
         * @param[in] objPath   - The Dbus object path
         * @param[in] version   - The Host version
         */
        Version(sdbusplus::bus::bus& bus,
                const std::string& objPath,
                const std::string& versionId) : VersionInherit(
                    bus, (objPath + '/' + getId(versionId)).c_str(), true)
        {
            // Set properties.
            purpose(VersionPurpose::Host);
            version(versionId);

            // Emit deferred signal.
            emit_object_added();
        }

        /**
         * @brief Get the code version identifier.
         *
         * @return The version identifier.
         **/
        static const std::string getVersion();

    private:
        /**
         * @brief Get the Host Version id.
         *
         * @return The id.
         **/
        const std::string getId(std::string version) const;
};

} // namespace manager
} // namespace software
} // namespace openpower

