#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <phosphor-logging/log.hpp>
#include "version.hpp"
#include <phosphor-logging/elog-errors.hpp>
#include "xyz/openbmc_project/Common/error.hpp"
#include "item_updater.hpp"
#include <openssl/sha.h>

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
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Version"),
                              Argument::ARGUMENT_VALUE(version.c_str()));
    }

    unsigned char digest[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, version.c_str(), strlen(version.c_str()));
    SHA512_Final(digest, &ctx);
    char mdString[SHA512_DIGEST_LENGTH*2+1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i*2], 3, "%02x", (unsigned int)digest[i]);
    }

    // Only need 8 hex digits.
    std::string hexId = std::string(mdString);
    return (hexId.substr(0, 8));
}

std::map<std::string, std::string> Version::getValue(
        const std::string& filePath, std::map<std::string, std::string> keys)
{
    if (filePath.empty())
    {
        log<level::ERR>("Error filePath is empty");
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FilePath"),
                              Argument::ARGUMENT_VALUE(filePath.c_str()));
    }

    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit |
                     std::ifstream::badbit |
                     std::ifstream::eofbit);

    try
    {
        efile.open(filePath);
        while (getline(efile, line))
        {
            for(auto& key : keys)
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

} // namespace updater
} // namespace software
} // namespace openpower
