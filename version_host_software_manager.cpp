#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include "version_host_software_manager.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

const std::string Version::getVersion()
{
    constexpr const auto versionKey = "version=";
    constexpr const auto versionKeySize = strlen(versionKey);
    std::string version{};
    std::ifstream efile;
    std::string line;
    efile.open("/tmp/pnor/pnor.toc");

    while (getline(efile, line))
    {
        if (line.compare(0, versionKeySize, versionKey) == 0)
        {
            // This line looks like:
            // version=open-power-witherspoon-v1.14-89-ga42959a.
            version = line.substr(versionKeySize);
            break;
        }
    }
    efile.close();
    return version;
}

const std::string Version::getId(std::string version) const
{
    std::stringstream hexId;

    if (version.empty())
    {
        throw std::runtime_error("Host version is empty");
    }

    // Only want 8 hex digits.
    hexId << std::hex << ((std::hash<std::string> {}(version)) & 0xFFFFFFFF);
    return hexId.str();
}

} // namespace manager
} // namespace software
} // namepsace openpower

