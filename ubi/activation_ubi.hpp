#pragma once

#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

class RedundancyPriorityUbi : public RedundancyPriority
{
  public:
    /** @brief Constructs RedundancyPriorityUbi.
     *
     *  @param[in] bus    - The Dbus bus object
     *  @param[in] path   - The Dbus object path
     *  @param[in] parent - Parent object.
     *  @param[in] value  - The redundancyPriority value
     */
    RedundancyPriorityUbi(sdbusplus::bus::bus& bus, const std::string& path,
                          Activation& parent, uint8_t value) :
        RedundancyPriority(bus, path, parent, value)
    {
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
    /** @brief Constructs Activation Software Manager
     *
     * @param[in] bus    - The Dbus bus object
     * @param[in] path   - The Dbus object path
     * @param[in] parent - Parent object.
     * @param[in] versionId  - The software version id
     * @param[in] extVersion - The extended version
     * @param[in] activationStatus - The status of Activation
     * @param[in] assocs - Association objects
     */
    ActivationUbi(sdbusplus::bus::bus& bus, const std::string& path,
                  ItemUpdater& parent, std::string& versionId,
                  std::string& extVersion,
                  sdbusplus::xyz::openbmc_project::Software::server::
                      Activation::Activations activationStatus,
                  AssociationList& assocs) :
        Activation(bus, path, parent, versionId, extVersion, activationStatus,
                   assocs)
    {
    }
    virtual ~ActivationUbi() = default;

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
