// SPDX-License-Identifier: Apache-2.0

/**@file functions.cpp*/

#include "functions.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>
#include <sdeventplus/event.hpp>

#include <filesystem>
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
 * @brief Get file extensions for IBMCompatibleSystem
 *
 * IBM host firmware can be deployed as blobs (files) in a filesystem.  Host
 * firmware blobs for different values of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem are packaged with
 * different filename extensions.  getExtensionsForIbmCompatibleSystem
 * maintains the mapping from a given value of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem to an array of
 * filename extensions.
 *
 * If a mapping is found getExtensionsForIbmCompatibleSystem returns true and
 * the extensions parameter is reset with the map entry.  If no mapping is
 * found getExtensionsForIbmCompatibleSystem returns false and extensions is
 * unmodified.
 *
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem to host firmware blob
 * file extensions.
 * @param[in] ibmCompatibleSystem The names property of an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem
 * @param[out] extentions the host firmware blob file extensions
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
 * bootstraping in files with well-known names.  findLinks uses the provided
 * extensions argument to find host firmware blob files that require a
 * well-known name.  When a blob is found, issue the provided callback
 * (typically a function that will write a symlink).
 *
 * @param[in] hostFirmwareDirectory The directory in which findLinks should
 * look for host firmware blob files that need well-known names.
 * @param[in] extentions The extensions of the firmware blob files denote a
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
    std::filesystem::directory_iterator directoryIterator(hostFirmwareDirectory,
                                                          ec);
    if (ec)
    {
        makeCallback(errorCallback, hostFirmwareDirectory, ec);
        return;
    }

    // Create a symlink from HBB to the corresponding LID file if it exists
    static const auto hbbLid = "81e0065a.lid";
    auto hbbLidPath = hostFirmwareDirectory / hbbLid;
    if (std::filesystem::exists(hbbLidPath))
    {
        static const auto hbbName = "HBB";
        auto hbbLinkPath = hostFirmwareDirectory / hbbName;
        makeCallback(linkCallback, hbbLid, hbbLinkPath, errorCallback);
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
 * @brief Make callbacks on
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem instances.
 *
 * Look for an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem in the provided
 * argument and if found, issue the provided callback.
 *
 * @param[in] interfacesAndProperties the interfaces in which to look for an
 * instance of xyz.openbmc_project.Configuration.IBMCompatibleSystem
 * @param[in] callback the user callback to make if
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem is found in
 * interfacesAndProperties
 * @return true if interfacesAndProperties contained an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem, false otherwise
 */
bool maybeCall(const std::map<std::string,
                              std::map<std::string,
                                       std::variant<std::vector<std::string>>>>&
                   interfacesAndProperties,
               const MaybeCallCallbackType& callback)
{
    using namespace std::string_literals;

    static const auto interfaceName =
        "xyz.openbmc_project.Configuration.IBMCompatibleSystem"s;
    auto interfaceIterator = interfacesAndProperties.find(interfaceName);
    if (interfaceIterator == interfacesAndProperties.cend())
    {
        // IBMCompatibleSystem interface not found, so instruct the caller to
        // keep waiting or try again later.
        return false;
    }
    auto propertyIterator = interfaceIterator->second.find("Names"s);
    if (propertyIterator == interfaceIterator->second.cend())
    {
        // The interface exists but the property doesn't.  This is a bug in the
        // IBMCompatibleSystem implementation.  The caller should not try
        // again.
        std::cerr << "Names property not implemented on " << interfaceName
                  << "\n";
        return true;
    }

    const auto& ibmCompatibleSystem =
        std::get<std::vector<std::string>>(propertyIterator->second);
    if (callback)
    {
        callback(ibmCompatibleSystem);
    }

    // IBMCompatibleSystem found and callback issued.
    return true;
}

/**
 * @brief Make callbacks on
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem instances.
 *
 * Look for an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem in the provided
 * argument and if found, issue the provided callback.
 *
 * @param[in] message the DBus message in which to look for an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem
 * @param[in] callback the user callback to make if
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem is found in
 * message
 * @return true if message contained an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem, false otherwise
 */
bool maybeCallMessage(sdbusplus::message::message& message,
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
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem, determine if
 * well-known names for host firmare blob files are necessary and if so, create
 * them.
 *
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem to host firmware blob
 * file extensions.
 * @param[in] hostFirmwareDirectory The directory in which findLinks should
 * look for host firmware blob files that need well-known names.
 * @param[in] ibmCompatibleSystem The names property of an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem
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
 * @brief process host firmware
 *
 * Allocate a callback context and register for DBus.ObjectManager Interfaces
 * added signals from entity manager.
 *
 * Check the current entity manager object tree for a
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem instance (entity
 * manager will be dbus activated if it is not running).  If one is found,
 * determine if symlinks need to be created and create them.  Instruct the
 * program event loop to exit.
 *
 * If no instance of xyz.openbmc_project.Configuration.IBMCompatibleSystem is
 * found return the callback context to main, where the program will sleep
 * until the callback is invoked one or more times and instructs the program
 * event loop to exit when
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem is added.
 *
 * @param[in] bus a DBus client connection
 * @param[in] extensionMap a map of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem to host firmware blob
 * file extensions.
 * @param[in] hostFirmwareDirectory The directory in which processHostFirmware
 * should look for blob files.
 * @param[in] errorCallback A callback made in the event of filesystem errors.
 * @param[in] loop a program event loop
 * @return nullptr if an instance of
 * xyz.openbmc_project.Configuration.IBMCompatibleSystem is found, otherwise a
 * pointer to an sdbusplus match object.
 */
std::shared_ptr<void> processHostFirmware(
    sdbusplus::bus::bus& bus,
    std::map<std::string, std::vector<std::string>> extensionMap,
    std::filesystem::path hostFirmwareDirectory,
    ErrorCallbackType errorCallback, sdeventplus::Event& loop)
{
    // ownership of extensionMap, hostFirmwareDirectory and errorCallback can't
    // be transfered to the match callback because they are needed in the non
    // async part of this function below, so they need to be moved to the heap.
    auto pExtensionMap =
        std::make_shared<decltype(extensionMap)>(std::move(extensionMap));
    auto pHostFirmwareDirectory =
        std::make_shared<decltype(hostFirmwareDirectory)>(
            std::move(hostFirmwareDirectory));
    auto pErrorCallback =
        std::make_shared<decltype(errorCallback)>(std::move(errorCallback));

    // register for a callback in case the IBMCompatibleSystem interface has
    // not yet been published by entity manager.
    auto interfacesAddedMatch = std::make_shared<sdbusplus::bus::match::match>(
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
            // xyz.openbmc_project.Configuration.IBMCompatibleSystem, check to
            // see if links are necessary on this system and if so, create
            // them.
            if (maybeCallMessage(message, maybeMakeLinksWithArgsBound))
            {
                // The IBMCompatibleSystem interface was found and the links
                // were created if applicable.  Instruct the event loop /
                // subcommand to exit.
                loop.exit(0);
            }
        });

    // now that we'll get a callback in the event of an InterfacesAdded signal
    // (potentially containing
    // xyz.openbmc_project.Configuration.IBMCompatibleSystem), activate entity
    // manager if it isn't running and enumerate its objects
    auto getManagedObjects = bus.new_method_call(
        "xyz.openbmc_project.EntityManager", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    auto reply = bus.call(getManagedObjects);
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfacesAndProperties;
    std::map<sdbusplus::message::object_path, decltype(interfacesAndProperties)>
        objects;
    reply.read(objects);

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
        // xyz.openbmc_project.Configuration.IBMCompatibleSystem, check to see
        // if links are necessary on this system and if so, create them
        if (maybeCall(interfacesAndProperties, maybeMakeLinksWithArgsBound))
        {
            // The IBMCompatibleSystem interface is already on the bus and the
            // links were created if applicable.  Instruct the event loop to
            // exit.
            loop.exit(0);
            // The match object isn't needed anymore, so destroy it on return.
            return nullptr;
        }
    }

    // The IBMCompatibleSystem interface has not yet been published.  Move
    // ownership of the match callback to the caller.
    return interfacesAddedMatch;
}
} // namespace process_hostfirmware
} // namespace functions
