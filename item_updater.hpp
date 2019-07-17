#pragma once

#include "activation.hpp"
#include "org/openbmc/Associations/server.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
#include <xyz/openbmc_project/Object/Enable/server.hpp>

namespace openpower
{
namespace software
{
namespace updater
{

using ItemUpdaterInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset,
    sdbusplus::org::openbmc::server::Associations,
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll>;
using GardResetInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset>;
using ObjectEnable = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Object::server::Enable>;
namespace MatchRules = sdbusplus::bus::match::rules;

using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

constexpr auto GARD_PATH = "/org/open_power/control/gard";
constexpr static auto volatilePath = "/org/open_power/control/volatile";

/** @class GardReset
 *  @brief OpenBMC GARD factory reset implementation.
 *  @details An implementation of xyz.openbmc_project.Common.FactoryReset under
 *  /org/openpower/control/gard.
 */
class GardReset : public GardResetInherit
{
  public:
    /** @brief Constructs GardReset.
     *
     * @param[in] bus    - The Dbus bus object
     * @param[in] path   - The Dbus object path
     */
    GardReset(sdbusplus::bus::bus& bus, const std::string& path) :
        GardResetInherit(bus, path.c_str(), true), bus(bus), path(path)
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_added(path.c_str(), interfaces);
    }

    virtual ~GardReset()
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_removed(path.c_str(), interfaces);
    }

  protected:
    // TODO Remove once openbmc/openbmc#1975 is resolved
    static constexpr auto interface = "xyz.openbmc_project.Common.FactoryReset";
    sdbusplus::bus::bus& bus;
    std::string path;

    /**
     * @brief GARD factory reset - clears the PNOR GARD partition.
     */
    virtual void reset() = 0;
};

/** @class ItemUpdater
 *  @brief Manages the activation of the host version items.
 */
class ItemUpdater : public ItemUpdaterInherit
{
  public:
    /** @brief Constructs ItemUpdater
     *
     * @param[in] bus    - The D-Bus bus object
     * @param[in] path   - The D-Bus path
     */
    ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
        ItemUpdaterInherit(bus, path.c_str()), bus(bus),
        versionMatch(bus,
                     MatchRules::interfacesAdded() +
                         MatchRules::path("/xyz/openbmc_project/software"),
                     std::bind(std::mem_fn(&ItemUpdater::createActivation),
                               this, std::placeholders::_1))
    {
    }

    virtual ~ItemUpdater() = default;

    /** @brief Sets the given priority free by incrementing
     *  any existing priority with the same value by 1. It will then continue
     *  to resolve duplicate priorities caused by this increase, by increasing
     *  the priority by 1 until there are no more duplicate values.
     *
     *  @param[in] value - The priority that needs to be set free.
     *  @param[in] versionId - The Id of the version for which we
     *                         are trying to free up the priority.
     *  @return None
     */
    virtual void freePriority(uint8_t value, const std::string& versionId) = 0;

    /**
     * @brief Create and populate the active PNOR Version.
     */
    virtual void processPNORImage() = 0;

    /** @brief Deletes version
     *
     *  @param[in] entryId - Id of the version to delete
     *
     *  @return - Returns true if the version is deleted.
     */
    virtual bool erase(std::string entryId);

    /**
     * @brief Erases any non-active pnor versions.
     */
    virtual void deleteAll() = 0;

    /** @brief Brings the total number of active PNOR versions to
     *         ACTIVE_PNOR_MAX_ALLOWED -1. This function is intended to be
     *         run before activating a new PNOR version. If this function
     *         needs to delete any PNOR version(s) it will delete the
     *         version(s) with the highest priority, skipping the
     *         functional PNOR version.
     *
     *  @return - Return if space is freed or not
     */
    virtual bool freeSpace() = 0;

    /** @brief Creates an active association to the
     *  newly active software image
     *
     * @param[in]  path - The path to create the association to.
     */
    virtual void createActiveAssociation(const std::string& path);

    /** @brief Updates the functional association to the
     *  new "running" PNOR image
     *
     * @param[in]  versionId - The id of the image to update the association to.
     */
    virtual void updateFunctionalAssociation(const std::string& versionId);

    /** @brief Removes the associations from the provided software image path
     *
     * @param[in]  path - The path to remove the association from.
     */
    virtual void removeAssociation(const std::string& path);

    /** @brief Persistent GardReset dbus object */
    std::unique_ptr<GardReset> gardReset;

    /** @brief Check whether the provided image id is the functional one
     *
     * @param[in] - versionId - The id of the image to check.
     *
     * @return - Returns true if this version is currently functional.
     */
    virtual bool isVersionFunctional(const std::string& versionId) = 0;

    /** @brief Persistent ObjectEnable D-Bus object */
    std::unique_ptr<ObjectEnable> volatileEnable;

  protected:
    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    virtual void createActivation(sdbusplus::message::message& msg);

    /** @brief Create Activation object */
    virtual std::unique_ptr<Activation> createActivationObject(
        const std::string& path, const std::string& versionId,
        const std::string& extVersion,
        sdbusplus::xyz::openbmc_project::Software::server::Activation::
            Activations activationStatus,
        AssociationList& assocs) = 0;

    /** @brief Create Version object */
    virtual std::unique_ptr<Version>
        createVersionObject(const std::string& objPath,
                            const std::string& versionId,
                            const std::string& versionString,
                            sdbusplus::xyz::openbmc_project::Software::server::
                                Version::VersionPurpose versionPurpose,
                            const std::string& filePath) = 0;

    /** @brief Validate if image is valid or not */
    virtual bool validateImage(const std::string& path) = 0;

    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Activation>> activations;

    /** @brief Persistent map of Version D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<Version>> versions;

    /** @brief sdbusplus signal match for Software.Version */
    sdbusplus::bus::match_t versionMatch;

    /** @brief This entry's associations */
    AssociationList assocs = {};

    /** @brief Host factory reset - clears PNOR partitions for each
     * Activation D-Bus object */
    void reset() override = 0;

    /** @brief Check whether the host is running
     *
     * @return - Returns true if the Chassis is powered on.
     */
    bool isChassisOn();
};

} // namespace updater
} // namespace software
} // namespace openpower
