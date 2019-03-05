#pragma once

#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

class GardResetUbi : public GardReset
{
  public:
    GardResetUbi(sdbusplus::bus::bus& bus, const std::string& path) :
        GardReset(bus, path)
    {
    }

    virtual ~GardResetUbi() = default;

  protected:
    /**
     * @brief GARD factory reset - clears the PNOR GARD partition.
     */
    void reset() override;
};

/** @class ItemUpdaterUbi
 *  @brief Manages the activation of the host version items for ubi layout
 */
class ItemUpdaterUbi : public ItemUpdater
{
  public:
    ItemUpdaterUbi(sdbusplus::bus::bus& bus, const std::string& path) :
        ItemUpdater(bus, path)
    {
        processPNORImage();
        gardReset = std::make_unique<GardResetUbi>(bus, GARD_PATH);
        volatileEnable = std::make_unique<ObjectEnable>(bus, volatilePath);

        // Emit deferred signal.
        emit_object_added();
    }
    virtual ~ItemUpdaterUbi() = default;

    void freePriority(uint8_t value, const std::string& versionId) override;

    void processPNORImage() override;

    bool erase(std::string entryId) override;

    void deleteAll() override;

    bool freeSpace() override;

    bool isVersionFunctional(const std::string& versionId) override;

    /** @brief Determine the software version id
     *         from the symlink target (e.g. /media/ro-2a1022fe).
     *
     * @param[in] symlinkPath - The path of the symlink.
     * @param[out] id - The version id as a string.
     */
    static std::string determineId(const std::string& symlinkPath);

  private:
    std::unique_ptr<Activation> createActivationObject(
        const std::string& path, const std::string& versionId,
        const std::string& extVersion,
        sdbusplus::xyz::openbmc_project::Software::server::Activation::
            Activations activationStatus,
        AssociationList& assocs) override;

    std::unique_ptr<Version>
        createVersionObject(const std::string& objPath,
                            const std::string& versionId,
                            const std::string& versionString,
                            sdbusplus::xyz::openbmc_project::Software::server::
                                Version::VersionPurpose versionPurpose,
                            const std::string& filePath) override;

    bool validateImage(const std::string& path) override;

    /** @brief Host factory reset - clears PNOR partitions for each
     * Activation D-Bus object */
    void reset() override;

    /**
     * @brief Validates the presence of SquashFS image in the image dir.
     *
     * @param[in]  filePath - The path to the SquashFS image.
     * @param[out] result    - 0 --> if validation was successful
     *                       - -1--> Otherwise
     */
    static int validateSquashFSImage(const std::string& filePath);

    /** @brief Clears read only PNOR partition for
     *  given Activation D-Bus object
     *
     * @param[in]  versionId - The id of the ro partition to remove.
     */
    void removeReadOnlyPartition(const std::string& versionId);

    /** @brief Clears read write PNOR partition for
     *  given Activation D-Bus object
     *
     *  @param[in]  versionId - The id of the rw partition to remove.
     */
    void removeReadWritePartition(const std::string& versionId);

    /** @brief Clears preserved PNOR partition */
    void removePreservedPartition();
};

} // namespace updater
} // namespace software
} // namespace openpower
