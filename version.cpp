#include "version.hpp"

#include "item_updater.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <openssl/sha.h>

#include <fstream>
#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

namespace openpower
{
namespace software
{
namespace updater
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
using Argument = xyz::openbmc_project::Common::InvalidArgument;

std::string Version::getId(const std::string& version)
{

    if (version.empty())
    {
        log<level::ERR>("Error version is empty");
        return {};
    }

    unsigned char digest[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, version.c_str(), strlen(version.c_str()));
    SHA512_Final(digest, &ctx);
    char mdString[SHA512_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i * 2], 3, "%02x", (unsigned int)digest[i]);
    }

    // Only need 8 hex digits.
    std::string hexId = std::string(mdString);
    return (hexId.substr(0, 8));
}

std::map<std::string, std::string>
    Version::getValue(const std::string& filePath,
                      std::map<std::string, std::string> keys)
{
    if (filePath.empty())
    {
        log<level::ERR>("Error filePath is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FilePath"),
                              Argument::ARGUMENT_VALUE(filePath.c_str()));
    }

    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit | std::ifstream::badbit |
                     std::ifstream::eofbit);

    try
    {
        efile.open(filePath);
        while (getline(efile, line))
        {
            for (auto& key : keys)
            {
                auto value = key.first + "=";
                auto keySize = value.length();
                if (line.compare(0, keySize, value) == 0)
                {
                    key.second = line.substr(keySize);
                    break;
                }
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        if (!efile.eof())
        {
            log<level::ERR>("Error in reading file");
        }
        efile.close();
    }

    return keys;
}

std::pair<std::string, std::string>
    Version::getVersions(const std::string& versionPart)
{
    // versionPart contains strings like below:
    // open-power-romulus-v2.2-rc1-48-g268344f-dirty
    //     buildroot-2018.11.1-7-g5d7cc8c
    //     skiboot-v6.2
    std::istringstream iss(versionPart);
    std::string line;
    std::string version;
    std::stringstream ss;
    std::string extendedVersion;

    if (!std::getline(iss, line))
    {
        log<level::ERR>("Unable to read from version",
                        entry("VERSION=%s", versionPart.c_str()));
        return {};
    }
    version = line;

    while (std::getline(iss, line))
    {
        // Each line starts with a tab, let's trim it
        line.erase(line.begin(),
                   std::find_if(line.begin(), line.end(),
                                [](int c) { return !std::isspace(c); }));
        ss << line << ',';
    }
    extendedVersion = ss.str();

    // Erase the last ',', if there is one
    if (!extendedVersion.empty())
    {
        extendedVersion.pop_back();
    }
    return {version, extendedVersion};
}

void Delete::delete_()
{
    if (parent.eraseCallback)
    {
        parent.eraseCallback(parent.getId(parent.version()));
    }
}

void Version::updateDeleteInterface(sdbusplus::message::message& msg)
{
    std::string interface, chassisState;
    std::map<std::string, sdbusplus::message::variant<std::string>> properties;

    msg.read(interface, properties);

    for (const auto& p : properties)
    {
        if (p.first == "CurrentPowerState")
        {
            chassisState = std::get<std::string>(p.second);
        }
    }
    if (chassisState.empty())
    {
        // The chassis power state property did not change, return.
        return;
    }

    if ((parent.isVersionFunctional(this->versionId)) &&
        (chassisState != CHASSIS_STATE_OFF))
    {
        if (deleteObject)
        {
            deleteObject.reset(nullptr);
        }
    }
    else
    {
        if (!deleteObject)
        {
            deleteObject = std::make_unique<Delete>(bus, objPath, *this);
        }
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
