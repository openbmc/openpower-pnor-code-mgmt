#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <phosphor-logging/log.hpp>
#include "version.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;

std::string Version::getId(const std::string& version)
{
    std::stringstream hexId;

    if (version.empty())
    {
        log<level::ERR>("Error version is empty");
        throw std::runtime_error("Version is empty");
    }

    // Only want 8 hex digits.
    hexId << std::hex << ((std::hash<std::string> {}(
                               version)) & 0xFFFFFFFF);
    return hexId.str();
}

std::string Version::getValue(const std::string& filePath,
                              std::string key)
{
    key = key + "=";
    auto keySize = key.length();

    if (filePath.empty())
    {
        log<level::ERR>("Error filePath is empty");
        throw std::runtime_error("filePath is empty");
    }

    std::string value{};
    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit
                     | std::ifstream::badbit
                     | std::ifstream::eofbit);

    try
    {
        efile.open(filePath);
        while (getline(efile, line))
        {
            if (line.compare(0, keySize, key) == 0)
            {
                value = line.substr(keySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading file");
    }

    return value;
}

} // namespace updater
} // namespace software
} // namespace openpower
