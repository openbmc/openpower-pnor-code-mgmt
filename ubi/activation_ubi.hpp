#pragma once

#include "activation.hpp"

#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{

constexpr auto squashFSImage = "pnor.xz.squashfs";
using namespace phosphor::logging;

class RedundancyPriorityUbi : public RedundancyPriority
{
  public:
    RedundancyPriorityUbi(sdbusplus::bus::bus& bus, const std::string& path,
                          Activation& parent, uint8_t value) :
        RedundancyPriority(bus, path, parent, value)
    {
        priority(value);
    }
    virtual ~RedundancyPriorityUbi() = default;

    /** @brief Overloaded Priority property set function
     *
     *  @param[in] value - uint8_t
     *
     *  @return Success or exception thrown
     */
    uint8_t priority(uint8_t value) override;
};

/** @class ActivationUbi
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.Activation DBus API.
 */
class ActivationUbi : public Activation
{
  public:
    using Activation::Activation;
    virtual ~ActivationUbi() = default;

    /** @brief Overloaded Activation property setter function
     *
     *  @param[in] value - One of Activation::Activations
     *
     *  @return Success or exception thrown
     */
    Activations activation(Activations value) override;

    RequestedActivations
        requestedActivation(RequestedActivations value) override;

  private:
    /** @brief Tracks whether the read-only & read-write volumes have been
     *created as part of the activation process. **/
    bool ubiVolumesCreated = false;

    void unitStateChange(sdbusplus::message::message& msg) override;
    void startActivation() override;
    void finishActivation() override;
};

} // namespace updater
} // namespace software
} // namespace openpower
