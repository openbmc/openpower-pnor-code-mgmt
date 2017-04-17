#pragma once

#include <sdbusplus/server.hpp>
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
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   std::string& versionId) :
                   ActivationInherit(bus, path.c_str()),
                   busActivation(bus),
                   pathActivation(path),
                   versionId(versionId) {}

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

        /** @brief Persistent sdbusplus DBus bus connection */
        sdbusplus::bus::bus& busActivation;

        /** @brief Persistent DBus object path */
        std::string pathActivation;

        /** @brief Version id */
        std::string versionId;

        /** @brief Persistent map of ActivationBlocksTransition dbus objects
          *        and the version id */
        std::map<std::string, std::unique_ptr<ActivationBlocksTransition>>
                activationBlocksTransitions;
};

} // namespace updater
} // namespace software
} // namespace openpower

