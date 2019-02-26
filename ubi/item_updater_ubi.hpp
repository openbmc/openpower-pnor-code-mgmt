#pragma once

#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

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
        gardReset = std::make_unique<GardReset>(bus, GARD_PATH);
        volatileEnable = std::make_unique<ObjectEnable>(bus, volatilePath);

        // Emit deferred signal.
        emit_object_added();
    }
    virtual ~ItemUpdaterUbi() = default;

    void freePriority(uint8_t value, const std::string& versionId) override;

    bool isLowestPriority(uint8_t value) override;

    void processPNORImage() override;

    bool erase(std::string entryId) override;

    void deleteAll() override;

    void freeSpace() override;

    bool isVersionFunctional(const std::string& versionId) override;

    /** @brief Determine the software version id
     *         from the symlink target (e.g. /media/ro-2a1022fe).
     *
     * @param[in] symlinkPath - The path of the symlink.
     * @param[out] id - The version id as a string.
     */
    static std::string determineId(const std::string& symlinkPath);

  private:
    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void createActivation(sdbusplus::message::message& msg) override;

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
    void removeReadOnlyPartition(std::string versionId);

    /** @brief Clears read write PNOR partition for
     *  given Activation D-Bus object
     *
     *  @param[in]  versionId - The id of the rw partition to remove.
     */
    void removeReadWritePartition(std::string versionId);

    /** @brief Clears preserved PNOR partition */
    void removePreservedPartition();
};

} // namespace updater
} // namespace software
} // namespace openpower
