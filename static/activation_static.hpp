#pragma once

#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

/** @class ActivationStatic
 *  @brief Implementation for static PNOR layout
 */
class ActivationStatic : public Activation
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
    ActivationStatic(sdbusplus::bus::bus& bus, const std::string& path,
                     ItemUpdater& parent, const std::string& versionId,
                     const std::string& extVersion,
                     sdbusplus::xyz::openbmc_project::Software::server::
                         Activation::Activations activationStatus,
                     AssociationList& assocs) :
        Activation(bus, path, parent, versionId, extVersion, activationStatus,
                   assocs)
    {
    }
    ~ActivationStatic() = default;
    Activations activation(Activations value) override;

  private:
    void unitStateChange(sdbusplus::message::message& msg) override;
    void startActivation() override;
    void finishActivation() override;

    std::string pnorUpdateUnit;
};

} // namespace updater
} // namespace software
} // namespace openpower
