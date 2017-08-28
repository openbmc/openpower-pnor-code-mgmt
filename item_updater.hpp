#pragma once

#include <sdbusplus/server.hpp>
#include "activation.hpp"
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
#include "version.hpp"
#include "org/openbmc/Associations/server.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

using ItemUpdaterInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset,
    sdbusplus::org::openbmc::server::Associations>;
namespace MatchRules = sdbusplus::bus::match::rules;

using AssociationList =
        std::vector<std::tuple<std::string, std::string, std::string>>;

/** @class ItemUpdater
 *  @brief Manages the activation of the version items.
 */
class ItemUpdater : public ItemUpdaterInherit
{
    public:
        /** @brief Constructs ItemUpdater
         *
         * @param[in] bus    - The Dbus bus object
         * @param[in] path   - The Dbus path
         */
        ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
                    ItemUpdaterInherit(bus, path.c_str()),
                    bus(bus),
                    versionMatch(
                            bus,
                            MatchRules::interfacesAdded() +
                            MatchRules::path("/xyz/openbmc_project/software"),
                            std::bind(
                                    std::mem_fn(&ItemUpdater::createActivation),
                                    this,
                                    std::placeholders::_1))
        {
            processPNORImage();
        }

        /** @brief Sets the given priority free by incrementing
         *  any existing priority with the same value by 1
         *
         *  @param[in] value - The priority that needs to be set free.
         *  @param[in] versionId - The Id of the version for which we
         *                         are trying to free up the priority.
         *  @return None
         */
        void freePriority(uint8_t value, const std::string& versionId);

        /** @brief Determine is the given priority is the lowest
         *
         *  @param[in] value - The priority that needs to be checked.
         *
         *  @return boolean corresponding to whether the given
         *           priority is lowest.
         */
        bool isLowestPriority(uint8_t value);

        /**
         * @brief Create and populate the active PNOR Version.
         */
        void processPNORImage();

        /** @brief Deletes version
         *
         *  @param[in] entryId - Id of the version to delete
         *
         *  @return None
         */
        void erase(std::string entryId);

        /** @brief Deletes the active pnor version with highest priority
                   if the total number of volume exceeds the threshold.
         */
        void freeSpace();

        /** @brief Creates an active association to the
         *  newly active software image
         *
         * @param[in]  path - The path to create the association to.
         */
        void createActiveAssociation(std::string path);

        /** @brief Removes an active association to the software image
         *
         * @param[in]  path - The path to remove the association from.
         */
        void removeActiveAssociation(std::string path);

    private:
        /** @brief Callback function for Software.Version match.
         *  @details Creates an Activation dbus object.
         *
         * @param[in]  msg       - Data associated with subscribed signal
         */
        void createActivation(sdbusplus::message::message& msg);

        /**
         * @brief Validates the presence of SquashFS iamge in the image dir.
         *
         * @param[in]  filePath - The path to the SquashfFS image.
         * @param[out] result    - 0 --> if validation was successful
         *                       - -1--> Otherwise
         */
        static int validateSquashFSImage(const std::string& filePath);

        /** @brief Persistent sdbusplus DBus bus connection. */
        sdbusplus::bus::bus& bus;

        /** @brief Persistent map of Activation dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Activation>> activations;

        /** @brief Persistent map of Version dbus objects and their
          * version id */
        std::map<std::string, std::unique_ptr<Version>> versions;

        /** @brief sdbusplus signal match for Software.Version */
        sdbusplus::bus::match_t versionMatch;

        /** @brief This entry's associations */
        AssociationList assocs = {};

        /** @brief Clears read only PNOR partition for
         *  given Activation dbus object
         *
         * @param[in]  versionId - The id of the ro partition to remove.
         */
        void removeReadOnlyPartition(std::string versionId);

        /** @brief Clears read write PNOR partition for
         *  given Activation dbus object
         *
         * @param[in]  versionId - The id of the rw partition to remove.
         */
        void removeReadWritePartition(std::string versionId);

        /** @brief Clears preserved PNOR partition */
        void removePreservedPartition();

        /** @brief Host factory reset - clears PNOR partitions for each
          * Activation dbus object */
        void reset() override;
};

} // namespace updater
} // namespace software
} // namespace openpower
