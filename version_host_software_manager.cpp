#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include "version_host_software_manager.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

const std::string Version::getVersion()
{
    constexpr auto versionKey = "version=";
    constexpr auto versionKeySize = strlen(versionKey);
    constexpr auto tocFilePath = "/tmp/pnor/pnor.toc";

    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit
                     | std::ifstream::badbit
                     | std::ifstream::eofbit);

    // Too many GCC bugs (53984, 66145) to do this the right way...
    try
    {
        efile.open(tocFilePath);
        while (getline(efile, line))
        {
            if (line.compare(0, versionKeySize, versionKey) == 0)
            {
                version = line.substr(versionKeySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading Host pnor.toc file");
    }

    return version;
}

const std::string Version::getId(const std::string& version)
{
    std::stringstream hexId;

    if (version.empty())
    {
        log<level::ERR>("Error Host version is empty");
    }
    else
    {
        // Only want 8 hex digits.
        hexId << std::hex << ((std::hash<std::string> {}(
                                   version)) & 0xFFFFFFFF);
    }
    return hexId.str();
}

} // namespace manager
} // namespace software
} // namepsace openpower

