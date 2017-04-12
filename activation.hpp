#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/server.hpp>

namespace openpower
{
namespace software
{
namespace updater
{

using ActivationInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;
using ActivationBlocksTransitionInherit = sdbusplus::server::object::object<
 sdbusplus::xyz::openbmc_project::Software::server::ActivationBlocksTransition>;

/** @class Activation
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.Activation DBus API.
 */
class Activation : public ActivationInherit
{
    public:
        /** @brief Constructs Activation Software Manager
         *
         * @param[in] bus    - The Dbus bus object
         * @param[in] path   - The Dbus object path
         * @param[in] versionId - The software version id
         * @param[in] activationStatus - The status of Activation
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   std::string& versionId,
                   sdbusplus::xyz::openbmc_project::Software::
                   server::Activation::Activations activationStatus) :
                   ActivationInherit(bus, path.c_str(), true),
                   versionId(versionId)
        {
            // Set Properties.
            activation(activationStatus);
            // Emit deferred signal.
            emit_object_added();
        }

        /** @brief Overloaded Activation property setter function
         *
         *  @param[in] value - One of Activation::Activations
         *
         *  @return Success or exception thrown
         */
        Activations activation(Activations value) override;

        /** @brief Overloaded requestedActivation property setter function
         *
         *  @param[in] value - One of Activation::RequestedActivations
         *
         *  @return Success or exception thrown
         */
        RequestedActivations requestedActivation(RequestedActivations value)
                override;

        /** @brief Version id */
        std::string versionId;
};

/** @class ActivationBlocksTransition
 *  @brief OpenBMC ActivationBlocksTransition implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.ActivationBlocksTransition DBus API.
 */
class ActivationBlocksTransition : public ActivationBlocksTransitionInherit
{
    public:
        /** @brief Constructs ActivationBlocksTransition.
         *
         * @param[in] bus    - The Dbus bus object
         * @param[in] path   - The Dbus object path
         */
        ActivationBlocksTransition(sdbusplus::bus::bus& bus,
                                   const std::string& path) :
                   ActivationBlocksTransitionInherit(bus, path.c_str()) {}
};

} // namespace updater
} // namespace software
} // namespace openpower

