// SPDX-License-Identifier: Apache-2.0

/**@file functions.cpp*/

#include "config.h"

#include "functions.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>
#include <sdeventplus/event.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace functions
{
namespace process_hostfirmware
{

using namespace phosphor::logging;
using InterfacesPropertiesMap =
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>;
using ManagedObjectType =
    std::map<sdbusplus::message::object_path, InterfacesPropertiesMap>;

constexpr auto tocName = "pnor.toc";

/**
 * @brief Returns the managed objects for a given service
 */
ManagedObjectType getManagedObjects(sdbusplus::bus_t& bus,
                                    const std::string& service,
                                    const std::string& managerPath)

{
    auto method = bus.new_method_call(service.c_str(), managerPath.c_str(),
                                      "org.freedesktop.DBus.ObjectManager",
                                      "GetManagedObjects");

    ManagedObjectType objects;

    try
    {
        auto reply = bus.call(method);
        reply.read(objects);
    }
    catch (const sdbusplus::exception_t& e)
    {
        return ManagedObjectType{};
    }
    return objects;
}

/**
 * @brief Issue callbacks safely
 *
 * std::function can be empty, so this wrapper method checks for that prior to
 * calling it to avoid std::bad_function_call
 *
 * @tparam Sig the types of the std::function arguments
 * @tparam Args the deduced argument types
 * @param[in] callback the callback being wrapped
 * @param[in] args the callback arguments
 */
template <typename... Sig, typename... Args>
void makeCallback(const std::function<void(Sig...)>& callback, Args&&... args)
{
    if (callback)
    {
        callback(std::forward<Args>(args)...);
    }
}

/**
 * @brief Get file extensions for Compatible
 *
 * IBM host firmware can be deployed as blobs (files) in a filesystem. Host
 * firmware blobs for different values of
 * xyz.openbmc_project.Inventory.Decorator.Compatible are packaged with
 * different filename extensions. getExtensionsForIbmCompatibleSystem maintains
 * the mapping from a given value of
 * xyz.openbmc_project.Inventory.Decorator.Compatible to an array of filename
 * extensions.
 *
 * If a mapping is found getExtensionsForIbmCompatibleSystem returns true and
 * the extensions parameter is reset with the map entry. If no mapping is found
 * getExtensionsForIbmCompatibleSystem returns false and extensions is
 * unmodified.
 *
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Inventory.Decorator.Compatible to host firmware blob file
 * extensions.
 * @param[in] ibmCompatibleSystem The names property of an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible
 * @param[out] extensions the host firmware blob file extensions
 * @return true if an entry was found, otherwise false
 */
bool getExtensionsForIbmCompatibleSystem(
    const std::map<std::string, std::vector<std::string>>& extensionMap,
    const std::vector<std::string>& ibmCompatibleSystem,
    std::vector<std::string>& extensions)
{
    for (const auto& system : ibmCompatibleSystem)
    {
        auto extensionMapIterator = extensionMap.find(system);
        if (extensionMapIterator != extensionMap.end())
        {
            extensions = extensionMapIterator->second;
            return true;
        }
    }

    return false;
}

/**
 * @brief Write host firmware well-known name
 *
 * A wrapper around std::filesystem::create_symlink that avoids EEXIST by
 * deleting any pre-existing file.
 *
 * @param[in] linkTarget The link target argument to
 * std::filesystem::create_symlink
 * @param[in] linkPath The link path argument to std::filesystem::create_symlink
 * @param[in] errorCallback A callback made in the event of filesystem errors.
 */
void writeLink(const std::filesystem::path& linkTarget,
               const std::filesystem::path& linkPath,
               const ErrorCallbackType& errorCallback)
{
    std::error_code ec;

    // remove files with the same name as the symlink to be created,
    // otherwise symlink will fail with EEXIST.
    if (!std::filesystem::remove(linkPath, ec))
    {
        if (ec)
        {
            makeCallback(errorCallback, linkPath, ec);
            return;
        }
    }

    std::filesystem::create_symlink(linkTarget, linkPath, ec);
    if (ec)
    {
        makeCallback(errorCallback, linkPath, ec);
        return;
    }
}

/**
 * @brief Find host firmware blob files that need well-known names
 *
 * The IBM host firmware runtime looks for data and/or additional code while
 * bootstrapping in files with well-known names. findLinks uses the provided
 * extensions argument to find host firmware blob files that require a
 * well-known name. When a blob is found, issue the provided callback
 * (typically a function that will write a symlink).
 *
 * @param[in] hostFirmwareDirectory The directory in which findLinks should
 * look for host firmware blob files that need well-known names.
 * @param[in] extensions The extensions of the firmware blob files denote a
 * host firmware blob file requires a well-known name.
 * @param[in] errorCallback A callback made in the event of filesystem errors.
 * @param[in] linkCallback A callback made when host firmware blob files
 * needing a well known name are found.
 */
void findLinks(const std::filesystem::path& hostFirmwareDirectory,
               const std::vector<std::string>& extensions,
               const ErrorCallbackType& errorCallback,
               const LinkCallbackType& linkCallback)
{
    std::error_code ec;
    std::filesystem::directory_iterator directoryIterator(
        hostFirmwareDirectory, ec);
    if (ec)
    {
        makeCallback(errorCallback, hostFirmwareDirectory, ec);
        return;
    }

    // Create a symlink for pnor.toc
    static const auto tocLid = "81e00994.lid";
    auto tocLidPath = hostFirmwareDirectory / tocLid;
    if (std::filesystem::exists(tocLidPath))
    {
        auto tocLinkPath = hostFirmwareDirectory / tocName;
        makeCallback(linkCallback, tocLid, tocLinkPath, errorCallback);
    }

    for (; directoryIterator != std::filesystem::end(directoryIterator);
         directoryIterator.increment(ec))
    {
        const auto& file = directoryIterator->path();
        if (ec)
        {
            makeCallback(errorCallback, file, ec);
            // quit here if the increment call failed otherwise the loop may
            // never finish
            break;
        }

        if (std::find(extensions.begin(), extensions.end(), file.extension()) ==
            extensions.end())
        {
            // this file doesn't have an extension or doesn't match any of the
            // provided extensions.
            continue;
        }

        auto linkPath(file.parent_path().append(
            static_cast<const std::string&>(file.stem())));

        makeCallback(linkCallback, file.filename(), linkPath, errorCallback);
    }
}

/**
 * @brief Parse the elements json file and construct a string with the data to
 *        be used to update the bios attribute table.
 *
 * @param[in] elementsJsonFilePath - The path to the host firmware json file.
 * @param[in] extensions - The extensions of the firmware blob files.
 */
std::string getBiosAttrStr(const std::filesystem::path& elementsJsonFilePath,
                           const std::vector<std::string>& extensions)
{
    std::string biosAttrStr{};

    std::ifstream jsonFile(elementsJsonFilePath.c_str());
    if (!jsonFile)
    {
        return {};
    }

    std::map<std::string, std::string> attr;
    auto data = nlohmann::json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        log<level::ERR>("Error parsing JSON file",
                        entry("FILE=%s", elementsJsonFilePath.c_str()));
        return {};
    }

    // .get requires a non-const iterator
    for (auto& iter : data["lids"])
    {
        std::string name{};
        std::string lid{};

        try
        {
            name = iter["element_name"].get<std::string>();
            lid = iter["short_lid_name"].get<std::string>();
        }
        catch (const std::exception& e)
        {
            // Possibly the element or lid name field was not found
            log<level::ERR>("Error reading JSON field",
                            entry("FILE=%s", elementsJsonFilePath.c_str()),
                            entry("ERROR=%s", e.what()));
            continue;
        }

        // The elements with the ipl extension have higher priority. Therefore
        // Use operator[] to overwrite value if an entry for it already exists,
        // and create a second entry with key name element_RT to specify it as
        // a runtime element.
        // Ex: if the JSON contains an entry A.P10 with lid name X, it'll create
        // and try A=X. If the JSON also contained an entry A.P10.iplTime with
        // lid name Y, the A entry would be overwritten to be A=Y and a second
        // entry A_RT=X would be created.
        constexpr auto iplExtension = ".iplTime";
        constexpr auto runtimeSuffix = "_RT";
        std::filesystem::path path(name);
        if (path.extension() == iplExtension)
        {
            // Some elements have an additional extension, ex: .P10.iplTime
            // Strip off the ipl extension with stem(), then check if there is
            // an additional extension with extension().
            if (!path.stem().extension().empty())
            {
                // Check if the extension matches the extensions for this system
                if (std::find(extensions.begin(), extensions.end(),
                              path.stem().extension()) == extensions.end())
                {
                    continue;
                }
            }
            // Get the element name without extensions by calling stem() twice
            // since stem() returns the base name if no periods are found.
            // Therefore both "element.P10" and "element.P10.iplTime" would
            // become "element".
            auto keyName = path.stem().stem();
            auto attrIt = attr.find(keyName);
            if (attrIt != attr.end())
            {
                // Copy the existing entry to a runtime entry
                auto runtimeKeyName = keyName.string() + runtimeSuffix;
                attr.insert({runtimeKeyName, attrIt->second});
            }
            // Overwrite the existing element with the ipl entry
            attr[keyName] = lid;
            continue;
        }

        // Process all other extensions. The extension should match the list of
        // supported extensions for this system. Use .insert() to only add
        // entries that do not exist, so to not overwrite the values that may
        // had been added that had the ipl extension.
        if (std::find(extensions.begin(), extensions.end(), path.extension()) !=
            extensions.end())
        {
            auto keyName = path.stem();
            auto attrIt = attr.find(keyName);
            if (attrIt != attr.end())
            {
                // The existing entry is an ipl entry, therefore create this
                // entry as a runtime one.
                auto runtimeKeyName = keyName.string() + runtimeSuffix;
                attr.insert({runtimeKeyName, lid});
            }
            else
            {
                attr.insert({path.stem(), lid});
            }
        }
    }
    for (const auto& a : attr)
    {
        // Build the bios attribute string with format:
        // "element1=lid1,element2=lid2,elementN=lidN,"
        biosAttrStr += a.first + "=" + a.second + ",";

        std::error_code ec;
        auto lidName = a.second + ".lid";
        auto elementFilePath =
            std::filesystem::path("/media/hostfw/running") / a.first;

        // Remove the symlink if the target does not match so that it gets
        // recreated. Ignore pnor.toc, this symlink is manually created by the
        // function findLinks().
        if ((a.first != tocName) &&
            std::filesystem::is_symlink(elementFilePath, ec))
        {
            auto target = std::filesystem::read_symlink(elementFilePath, ec);
            if (target != lidName)
            {
                log<level::INFO>("Removing mismatched symlilnk",
                                 entry("LINK=%s", elementFilePath.c_str()),
                                 entry("TARGET=%s", target.c_str()),
                                 entry("EXPECTED:%s", lidName.c_str()));
                std::filesystem::remove(elementFilePath, ec);
            }
        }

        // Create symlinks from the hostfw elements to their corresponding
        // lid files if they don't exist
        if (!std::filesystem::exists(elementFilePath))
        {
            std::filesystem::create_symlink(lidName, elementFilePath, ec);
            if (ec)
            {
                log<level::ERR>("Error creating symlink",
                                entry("TARGET=%s", lidName.c_str()),
                                entry("LINK=%s", elementFilePath.c_str()));
            }
        }
    }

    // Delete the last comma of the bios attribute string
    if (biosAttrStr.back() == ',')
    {
        return biosAttrStr.substr(0, biosAttrStr.length() - 1);
    }

    return biosAttrStr;
}

/**
 * @brief Set the bios attribute table with details of the host firmware data
 * for this system.
 *
 * @param[in] elementsJsonFilePath - The path to the host firmware json file.
 * @param[in] extensions - The extensions of the firmware blob files.
 */
void setBiosAttr(const std::filesystem::path& elementsJsonFilePath,
                 const std::vector<std::string>& extensions)
{
    auto biosAttrStr = getBiosAttrStr(elementsJsonFilePath, extensions);

    constexpr auto biosConfigPath = "/xyz/openbmc_project/bios_config/manager";
    constexpr auto biosConfigIntf = "xyz.openbmc_project.BIOSConfig.Manager";
    constexpr auto dbusAttrName = "hb_lid_ids";
    constexpr auto dbusAttrType =
        "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.String";

    using PendingAttributesType = std::vector<std::pair<
        std::string, std::tuple<std::string, std::variant<std::string>>>>;
    PendingAttributesType pendingAttributes;
    pendingAttributes.emplace_back(std::make_pair(
        dbusAttrName, std::make_tuple(dbusAttrType, biosAttrStr)));

    auto bus = sdbusplus::bus::new_default();
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");
    method.append(biosConfigPath, std::vector<std::string>({biosConfigIntf}));
    std::vector<std::pair<std::string, std::vector<std::string>>> response;
    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty())
        {
            log<level::INFO>("Error reading mapper response",
                             entry("PATH=%s", biosConfigPath),
                             entry("INTERFACE=%s", biosConfigIntf));
            throw sdbusplus::xyz::openbmc_project::Common::Error::
                InternalFailure();
        }
        auto method = bus.new_method_call((response.begin()->first).c_str(),
                                          biosConfigPath,
                                          SYSTEMD_PROPERTY_INTERFACE, "Set");
        method.append(biosConfigIntf, "PendingAttributes",
                      std::variant<PendingAttributesType>(pendingAttributes));
        bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::INFO>("Error setting the bios attribute",
                         entry("ERROR=%s", e.what()),
                         entry("ATTRIBUTE=%s", dbusAttrName));
        throw;
    }
}

/**
 * @brief Make callbacks on
 * xyz.openbmc_project.Inventory.Decorator.Compatible instances.
 *
 * Look for an instance of xyz.openbmc_project.Inventory.Decorator.Compatible in
 * the provided argument and if found, issue the provided callback.
 *
 * @param[in] interfacesAndProperties the interfaces in which to look for an
 * instance of xyz.openbmc_project.Inventory.Decorator.Compatible
 * @param[in] callback the user callback to make if
 * xyz.openbmc_project.Inventory.Decorator.Compatible is found in
 * interfacesAndProperties
 * @return true if interfacesAndProperties contained an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible, false otherwise
 */
bool maybeCall(
    const std::map<
        std::string,
        std::map<std::string, std::variant<std::vector<std::string>>>>&
        interfacesAndProperties,
    const MaybeCallCallbackType& callback)
{
    using namespace std::string_literals;

    static const auto interfaceName =
        "xyz.openbmc_project.Inventory.Decorator.Compatible"s;
    auto interfaceIterator = interfacesAndProperties.find(interfaceName);
    if (interfaceIterator == interfacesAndProperties.cend())
    {
        // Compatible interface not found, so instruct the caller to keep
        // waiting or try again later.
        return false;
    }
    auto propertyIterator = interfaceIterator->second.find("Names"s);
    if (propertyIterator == interfaceIterator->second.cend())
    {
        // The interface exists but the property doesn't. This is a bug in the
        // Compatible implementation. The caller should not try again.
        std::cerr << "Names property not implemented on " << interfaceName
                  << "\n";
        return true;
    }

    const auto& ibmCompatibleSystem =
        std::get<std::vector<std::string>>(propertyIterator->second);
    if (callback)
    {
        try
        {
            callback(ibmCompatibleSystem);
        }
        catch (const sdbusplus::exception_t& e)
        {
            return false;
        }
    }

    // Compatible found and callback issued.
    return true;
}

/**
 * @brief Make callbacks on
 * xyz.openbmc_project.Inventory.Decorator.Compatible instances.
 *
 * Look for an instance ofxyz.openbmc_project.Inventory.Decorator.Compatible in
 * the provided argument and if found, issue the provided callback.
 *
 * @param[in] message the DBus message in which to look for an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible
 * @param[in] callback the user callback to make if
 * xyz.openbmc_project.Inventory.Decorator.Compatible is found in message
 * @return true if message contained an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible, false otherwise
 */
bool maybeCallMessage(sdbusplus::message_t& message,
                      const MaybeCallCallbackType& callback)
{
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfacesAndProperties;
    sdbusplus::message::object_path _;
    message.read(_, interfacesAndProperties);
    return maybeCall(interfacesAndProperties, callback);
}

/**
 * @brief Determine system support for host firmware well-known names.
 *
 * Using the provided extensionMap and
 * xyz.openbmc_project.Inventory.Decorator.Compatible, determine if well-known
 * names for host firmware blob files are necessary and if so, create them.
 *
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Inventory.Decorator.Compatible to host firmware blob file
 * extensions.
 * @param[in] hostFirmwareDirectory The directory in which findLinks should look
 * for host firmware blob files that need well-known names.
 * @param[in] ibmCompatibleSystem The names property of an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible
 * @param[in] errorCallback A callback made in the event of filesystem errors.
 */
void maybeMakeLinks(
    const std::map<std::string, std::vector<std::string>>& extensionMap,
    const std::filesystem::path& hostFirmwareDirectory,
    const std::vector<std::string>& ibmCompatibleSystem,
    const ErrorCallbackType& errorCallback)
{
    std::vector<std::string> extensions;
    if (getExtensionsForIbmCompatibleSystem(extensionMap, ibmCompatibleSystem,
                                            extensions))
    {
        findLinks(hostFirmwareDirectory, extensions, errorCallback, writeLink);
    }
}

/**
 * @brief Determine system support for updating the bios attribute table.
 *
 * Using the provided extensionMap and
 * xyz.openbmc_project.Inventory.Decorator.Compatible, determine if the bios
 * attribute table needs to be updated.
 *
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Inventory.Decorator.Compatible to host firmware blob file
 * extensions.
 * @param[in] elementsJsonFilePath The file path to the json file
 * @param[in] ibmCompatibleSystem The names property of an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible
 */
void maybeSetBiosAttr(
    const std::map<std::string, std::vector<std::string>>& extensionMap,
    const std::filesystem::path& elementsJsonFilePath,
    const std::vector<std::string>& ibmCompatibleSystem)
{
    std::vector<std::string> extensions;
    if (getExtensionsForIbmCompatibleSystem(extensionMap, ibmCompatibleSystem,
                                            extensions))
    {
        try
        {
            setBiosAttr(elementsJsonFilePath, extensions);
        }
        catch (const sdbusplus::exception_t& e)
        {
            throw;
        }
    }
}

/**
 * @brief process host firmware
 *
 * Allocate a callback context and register for DBus.ObjectManager Interfaces
 * added signals from entity manager.
 *
 * Check the current entity manager object tree for a
 * xyz.openbmc_project.Inventory.Decorator.Compatible instance (entity manager
 * will be dbus activated if it is not running). If one is found, determine if
 * symlinks need to be created and create them. Instruct the program event loop
 * to exit.
 *
 * If no instance of xyz.openbmc_project.Inventory.Decorator.Compatible is found
 * return the callback context to main, where the program will sleep until the
 * callback is invoked one or more times and instructs the program event loop to
 * exit when xyz.openbmc_project.Inventory.Decorator.Compatible is added.
 *
 * @param[in] bus a DBus client connection
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Inventory.Decorator.Compatible to host firmware blob file
 * extensions.
 * @param[in] hostFirmwareDirectory The directory in which processHostFirmware
 * should look for blob files.
 * @param[in] errorCallback A callback made in the event of filesystem errors.
 * @param[in] loop a program event loop
 * @return nullptr if an instance of
 * xyz.openbmc_project.Inventory.Decorator.Compatible is found, otherwise a
 * pointer to an sdbusplus match object.
 */
std::shared_ptr<void> processHostFirmware(
    sdbusplus::bus_t& bus,
    std::map<std::string, std::vector<std::string>> extensionMap,
    std::filesystem::path hostFirmwareDirectory,
    ErrorCallbackType errorCallback, sdeventplus::Event& loop)
{
    // ownership of extensionMap, hostFirmwareDirectory and errorCallback can't
    // be transferred to the match callback because they are needed in the non
    // async part of this function below, so they need to be moved to the heap.
    auto pExtensionMap =
        std::make_shared<decltype(extensionMap)>(std::move(extensionMap));
    auto pHostFirmwareDirectory =
        std::make_shared<decltype(hostFirmwareDirectory)>(
            std::move(hostFirmwareDirectory));
    auto pErrorCallback =
        std::make_shared<decltype(errorCallback)>(std::move(errorCallback));

    // register for a callback in case the Compatible interface has not yet been
    // published by entity manager.
    auto interfacesAddedMatch = std::make_shared<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded() +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        [pExtensionMap, pHostFirmwareDirectory, pErrorCallback,
         &loop](auto& message) {
            // bind the extension map, host firmware directory, and error
            // callback to the maybeMakeLinks function.
            auto maybeMakeLinksWithArgsBound =
                std::bind(maybeMakeLinks, std::cref(*pExtensionMap),
                          std::cref(*pHostFirmwareDirectory),
                          std::placeholders::_1, std::cref(*pErrorCallback));

            // if the InterfacesAdded message contains an an instance of
            // xyz.openbmc_project.Inventory.Decorator.Compatible, check to see
            // if links are necessary on this system and if so, create them.
            if (maybeCallMessage(message, maybeMakeLinksWithArgsBound))
            {
                // The Compatible interface was found and the links were created
                // if applicable. Instruct the event loop / subcommand to exit.
                loop.exit(0);
            }
        });

    // now that we'll get a callback in the event of an InterfacesAdded signal
    // (potentially containing
    // xyz.openbmc_project.Inventory.Decorator.Compatible), activate entity
    // manager if it isn't running and enumerate its objects
    auto getManagedObjects = bus.new_method_call(
        "xyz.openbmc_project.EntityManager", "/xyz/openbmc_project/inventory",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfacesAndProperties;
    std::map<sdbusplus::message::object_path, decltype(interfacesAndProperties)>
        objects;
    try
    {
        auto reply = bus.call(getManagedObjects);
        reply.read(objects);
    }
    catch (const sdbusplus::exception_t& e)
    {
        // Error querying the EntityManager interface. Return the match to have
        // the callback run if/when the interface appears in D-Bus.
        return interfacesAddedMatch;
    }

    // bind the extension map, host firmware directory, and error callback to
    // the maybeMakeLinks function.
    auto maybeMakeLinksWithArgsBound =
        std::bind(maybeMakeLinks, std::cref(*pExtensionMap),
                  std::cref(*pHostFirmwareDirectory), std::placeholders::_1,
                  std::cref(*pErrorCallback));

    for (const auto& pair : objects)
    {
        std::tie(std::ignore, interfacesAndProperties) = pair;
        // if interfacesAndProperties contains an an instance of
        // xyz.openbmc_project.Inventory.Decorator.Compatible, check to see if
        // links are necessary on this system and if so, create them
        if (maybeCall(interfacesAndProperties, maybeMakeLinksWithArgsBound))
        {
            // The Compatible interface is already on the bus and the links were
            // created if applicable. Instruct the event loop to exit.
            loop.exit(0);
            // The match object isn't needed anymore, so destroy it on return.
            return nullptr;
        }
    }

    // The Compatible interface has not yet been published. Move ownership of
    // the match callback to the caller.
    return interfacesAddedMatch;
}

/**
 * @brief Update the Bios Attribute Table
 *
 * If an instance of xyz.openbmc_project.Inventory.Decorator.Compatible is
 * found, update the Bios Attribute Table with the appropriate host firmware
 * data.
 *
 * @param[in] bus - D-Bus client connection.
 * @param[in] extensionMap - Map of Compatible names and host firmware file
                             extensions.
 * @param[in] elementsJsonFilePath - The Path to the json file
 * @param[in] loop - Program event loop.
 * @return nullptr
 */
std::vector<std::shared_ptr<void>> updateBiosAttrTable(
    sdbusplus::bus_t& bus,
    std::map<std::string, std::vector<std::string>> extensionMap,
    std::filesystem::path elementsJsonFilePath, sdeventplus::Event& loop)
{
    constexpr auto entityManagerServiceName =
        "xyz.openbmc_project.EntityManager";

    auto pExtensionMap =
        std::make_shared<decltype(extensionMap)>(std::move(extensionMap));
    auto pElementsJsonFilePath =
        std::make_shared<decltype(elementsJsonFilePath)>(
            std::move(elementsJsonFilePath));

    auto maybeSetAttrWithArgsBound =
        std::bind(maybeSetBiosAttr, std::cref(*pExtensionMap),
                  std::cref(*pElementsJsonFilePath), std::placeholders::_1);

    std::vector<std::shared_ptr<void>> matches;

    // Entity Manager is needed to get the list of supported extensions. Add a
    // match to monitor interfaces added in case it's not running yet.
    matches.emplace_back(std::make_shared<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded() +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        [pExtensionMap, pElementsJsonFilePath, maybeSetAttrWithArgsBound,
         &loop](auto& message) {
            if (maybeCallMessage(message, maybeSetAttrWithArgsBound))
            {
                loop.exit(0);
            }
        }));

    // The BIOS attribute table can only be updated if PLDM is running because
    // PLDM is the one that exposes this property. Add a match to monitor when
    // the PLDM service starts.
    matches.emplace_back(std::make_shared<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::nameOwnerChanged() +
            sdbusplus::bus::match::rules::arg0namespace(
                "xyz.openbmc_project.PLDM"),
        [pExtensionMap, pElementsJsonFilePath, maybeSetAttrWithArgsBound,
         &loop](auto& message) {
            std::string name;
            std::string oldOwner;
            std::string newOwner;
            message.read(name, oldOwner, newOwner);

            if (newOwner.empty())
            {
                return;
            }

            auto bus = sdbusplus::bus::new_default();
            InterfacesPropertiesMap interfacesAndProperties;
            auto objects = getManagedObjects(bus, entityManagerServiceName,
                                             "/xyz/openbmc_project/inventory");
            for (const auto& pair : objects)
            {
                std::tie(std::ignore, interfacesAndProperties) = pair;
                if (maybeCall(interfacesAndProperties,
                              maybeSetAttrWithArgsBound))
                {
                    loop.exit(0);
                }
            }
        }));

    InterfacesPropertiesMap interfacesAndProperties;
    auto objects = getManagedObjects(bus, entityManagerServiceName,
                                     "/xyz/openbmc_project/inventory");
    for (const auto& pair : objects)
    {
        std::tie(std::ignore, interfacesAndProperties) = pair;
        if (maybeCall(interfacesAndProperties, maybeSetAttrWithArgsBound))
        {
            loop.exit(0);
            return {};
        }
    }

    return matches;
}

} // namespace process_hostfirmware
} // namespace functions
