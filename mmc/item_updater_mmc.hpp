#pragma once

#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

class GardResetMMC : public GardReset
{
  public:
    using GardReset::GardReset;
    virtual ~GardResetMMC() = default;

    /**
     * @brief GARD factory reset - clears the PNOR GARD partition.
     */
    void reset() override;

  private:
    /**
     * @brief During host factory reset, host clears the guard records in the
     * guard partition. BMC is not notified about host factory reset so
     * it continues to disable the guarded items. Now modified to enable
     * back CpuCore and DIMM items during factory reset.
     */
    void enableInventoryItems();

    /**
     * @brief Helper function to set enable flag for all the subtree objects
     * implementing the specified interface.
     *
     * @param[in] - service D-Bus service name
     * @param[in] - intf find objects that implement this interface
     * @param[in] - objPath find subtree objects from this object path
     * @return none
     */
    void enableInventoryItemsHelper(const std::string& service,
                                    const std::string& intf,
                                    const std::string& objPath);
};

/** @class ItemUpdaterMMC
 *  @brief Manages the activation of the host version items for static layout
 */
class ItemUpdaterMMC : public ItemUpdater
{
  public:
    ItemUpdaterMMC(sdbusplus::bus::bus& bus, const std::string& path) :
        ItemUpdater(bus, path)
    {
        processPNORImage();
        gardReset = std::make_unique<GardResetMMC>(bus, GARD_PATH);
        volatileEnable = std::make_unique<ObjectEnable>(bus, volatilePath);

        // Emit deferred signal.
        emit_object_added();
    }
    virtual ~ItemUpdaterMMC() = default;

    void freePriority(uint8_t value, const std::string& versionId) override;

    void processPNORImage() override;

    void deleteAll() override;

    bool freeSpace() override;

    void updateFunctionalAssociation(const std::string& versionId) override;

    bool isVersionFunctional(const std::string& versionId) override;

  private:
    /** @brief Create Activation object */
    std::unique_ptr<Activation> createActivationObject(
        const std::string& path, const std::string& versionId,
        const std::string& extVersion,
        sdbusplus::xyz::openbmc_project::Software::server::Activation::
            Activations activationStatus,
        AssociationList& assocs) override;

    /** @brief Create Version object */
    std::unique_ptr<Version>
        createVersionObject(const std::string& objPath,
                            const std::string& versionId,
                            const std::string& versionString,
                            sdbusplus::xyz::openbmc_project::Software::server::
                                Version::VersionPurpose versionPurpose,
                            const std::string& filePath) override;

    /** @brief Validate if image is valid or not */
    bool validateImage(const std::string& path);

    /** @brief Host factory reset - clears PNOR partitions for each
     * Activation D-Bus object */
    void reset() override;

    /** @brief The functional version ID */
    std::string functionalVersionId;
};

} // namespace updater
} // namespace software
} // namespace openpower
