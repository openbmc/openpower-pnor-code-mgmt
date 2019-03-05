#pragma once

#include "config.h"

#include "xyz/openbmc_project/Common/FilePath/server.hpp"
#include "xyz/openbmc_project/Object/Delete/server.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <sdbusplus/bus.hpp>

namespace openpower
{
namespace software
{
namespace updater
{

class ItemUpdater;

typedef std::function<void(std::string)> eraseFunc;

using VersionInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Version,
    sdbusplus::xyz::openbmc_project::Common::server::FilePath>;
using DeleteInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Object::server::Delete>;

namespace sdbusRule = sdbusplus::bus::match::rules;

class Delete;
class Version;

/** @class Delete
 *  @brief OpenBMC Delete implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Object.Delete
 *  D-Bus API.
 */
class Delete : public DeleteInherit
{
  public:
    /** @brief Constructs Delete.
     *
     *  @param[in] bus    - The D-Bus bus object
     *  @param[in] path   - The D-Bus object path
     *  @param[in] parent - Parent object.
     */
    Delete(sdbusplus::bus::bus& bus, const std::string& path, Version& parent) :
        DeleteInherit(bus, path.c_str(), true), parent(parent), bus(bus),
        path(path)
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_added(path.c_str(), interfaces);
    }

    ~Delete()
    {
        std::vector<std::string> interfaces({interface});
        bus.emit_interfaces_removed(path.c_str(), interfaces);
    }

    /**
     * @brief Delete the D-Bus object.
     *        Overrides the default delete function by calling
     *        Version class erase Method.
     **/
    void delete_() override;

  private:
    /** @brief Parent Object. */
    Version& parent;

    // TODO Remove once openbmc/openbmc#1975 is resolved
    static constexpr auto interface = "xyz.openbmc_project.Object.Delete";
    sdbusplus::bus::bus& bus;
    std::string path;
};

/** @class Version
 *  @brief OpenBMC version software management implementation.
 *  @details A concrete implementation for xyz.openbmc_project.Software.Version
 *  D-Bus API.
 */
class Version : public VersionInherit
{
  public:
    /** @brief Constructs Version Software Manager.
     *
     * @param[in] bus            - The D-Bus bus object
     * @param[in] objPath        - The D-Bus object path
     * @param[in] parent         - Parent object.
     * @param[in] versionId      - The version Id
     * @param[in] versionString  - The version string
     * @param[in] versionPurpose - The version purpose
     * @param[in] filePath       - The image filesystem path
     * @param[in] callback       - The eraseFunc callback
     */
    Version(sdbusplus::bus::bus& bus, const std::string& objPath,
            ItemUpdater& parent, const std::string& versionId,
            const std::string& versionString, VersionPurpose versionPurpose,
            const std::string& filePath, eraseFunc callback) :
        VersionInherit(bus, (objPath).c_str(), true),
        eraseCallback(callback), bus(bus), objPath(objPath), parent(parent),
        versionId(versionId), versionStr(versionString),
        chassisStateSignals(
            bus,
            sdbusRule::type::signal() + sdbusRule::member("PropertiesChanged") +
                sdbusRule::path(CHASSIS_STATE_PATH) +
                sdbusRule::argN(0, CHASSIS_STATE_OBJ) +
                sdbusRule::interface(SYSTEMD_PROPERTY_INTERFACE),
            std::bind(std::mem_fn(&Version::updateDeleteInterface), this,
                      std::placeholders::_1))
    {
        // Set properties.
        purpose(versionPurpose);
        version(versionString);
        path(filePath);

        // Emit deferred signal.
        emit_object_added();
    }

    /**
     * @brief Update the Object.Delete interface for this activation
     *
     *        Update the delete interface based on whether or not this
     *        activation is currently functional. A functional activation
     *        will have no Object.Delete, while a non-functional activation
     *        will have one.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    void updateDeleteInterface(sdbusplus::message::message& msg);

    /**
     * @brief Read the manifest file to get the value of the key.
     *
     * @param[in] filePath - The path to the file which contains the value
     *                       of keys.
     * @param[in] keys     - A map of keys with empty values.
     *
     * @return The map of keys with filled values.
     **/
    static std::map<std::string, std::string>
        getValue(const std::string& filePath,
                 std::map<std::string, std::string> keys);

    /**
     * @brief Get version and extended version from VERSION partition string.
     *
     * @param[in] versionPart - The string containing the VERSION partition.
     *
     * @return The pair contains the version and extended version.
     **/
    static std::pair<std::string, std::string>
        getVersions(const std::string& versionPart);

    /**
     * @brief Calculate the version id from the version string.
     *
     * @details The version id is a unique 8 hexadecimal digit id
     *          calculated from the version string.
     *
     * @param[in] version - The image version string (e.g. v1.99.10-19).
     *
     * @return The id.
     */
    static std::string getId(const std::string& version);

    /** @brief Persistent Delete D-Bus object */
    std::unique_ptr<Delete> deleteObject;

    /** @brief The parent's erase callback. */
    eraseFunc eraseCallback;

  private:
    /** @brief Persistent sdbusplus DBus bus connection */
    sdbusplus::bus::bus& bus;

    /** @brief Persistent DBus object path */
    std::string objPath;

    /** @brief Parent Object. */
    ItemUpdater& parent;

    /** @brief This Version's version Id */
    const std::string versionId;

    /** @brief This Version's version string */
    const std::string versionStr;

    /** @brief Used to subscribe to chassis power state changes **/
    sdbusplus::bus::match_t chassisStateSignals;
};

} // namespace updater
} // namespace software
} // namespace openpower
