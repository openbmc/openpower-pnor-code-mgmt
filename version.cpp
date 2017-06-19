#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <phosphor-logging/log.hpp>
#include "version.hpp"
#include <phosphor-logging/elog-errors.hpp>
#include "xyz/openbmc_project/Common/error.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

std::string Version::getId(const std::string& version)
{
    std::stringstream hexId;

    if (version.empty())
    {
        log<level::ERR>("Error version is empty");
        elog<InvalidArgument>(xyz::openbmc_project::Common::InvalidArgument::
                              ARGUMENT_NAME("Version"),
                              xyz::openbmc_project::Common::InvalidArgument::
                              ARGUMENT_VALUE(version.c_str()));
    }

    // Only want 8 hex digits.
    hexId << std::hex << ((std::hash<std::string> {}(
                               version)) & 0xFFFFFFFF);
    return hexId.str();
}

std::vector<std::string> Version::getValue(
        const std::string& filePath, std::vector<std::string> keys)
{

    if (filePath.empty())
    {
        log<level::ERR>("Error filePath is empty");
        elog<InvalidArgument>(xyz::openbmc_project::Common::InvalidArgument::
                              ARGUMENT_NAME("FilePath"),
                              xyz::openbmc_project::Common::InvalidArgument::
                              ARGUMENT_VALUE(filePath.c_str()));
    }

    std::vector<std::string> keyValues;
    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit
                     | std::ifstream::badbit
                     | std::ifstream::eofbit);

    try
    {
        efile.open(filePath);
        for(auto key : keys)
        {
            key = key + "=";
            auto keySize = key.length();
            while (getline(efile, line))
            {
                if (line.compare(0, keySize, key) == 0)
                {
                    keyValues.push_back(line.substr(keySize));
                    break;
                }
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading file");
    }

    return keyValues;
}

} // namespace updater
} // namespace software
} // namespace openpower
