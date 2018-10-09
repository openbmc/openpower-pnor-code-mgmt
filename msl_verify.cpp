#include "config.h"

#include "msl_verify.hpp"

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <regex>

namespace openpower
{
namespace software
{
namespace image
{

namespace fs = std::experimental::filesystem;
using namespace phosphor::logging;

int MinimumShipLevel::compare(const Version& a, const Version& b)
{
    if (a.major < b.major)
    {
        return -1;
    }
    else if (a.major > b.major)
    {
        return 1;
    }

    if (a.minor < b.minor)
    {
        return -1;
    }
    else if (a.minor > b.minor)
    {
        return 1;
    }

    if (a.rev < b.rev)
    {
        return -1;
    }
    else if (a.rev > b.rev)
    {
        return 1;
    }

    return 0;
}

void MinimumShipLevel::parse(const std::string& versionStr, Version& version)
{
    std::smatch match;
    version = {0, 0, 0};

    // Match for vX.Y.Z
    std::regex regex{"v([0-9]+)\\.([0-9]+)\\.([0-9]+)", std::regex::extended};

    if (!std::regex_search(versionStr, match, regex))
    {
        // Match for vX.Y
        std::regex regexShort{"v([0-9]+)\\.([0-9]+)", std::regex::extended};
        if (!std::regex_search(versionStr, match, regexShort))
        {
            log<level::ERR>("Unable to parse PNOR version",
                            entry("VERSION=%s", versionStr.c_str()));
            return;
        }
    }
    else
    {
        // Populate Z
        version.rev = std::stoi(match[3]);
    }
    version.major = std::stoi(match[1]);
    version.minor = std::stoi(match[2]);
}

std::string MinimumShipLevel::getFunctionalVersion()
{
    if (!fs::exists(PNOR_RO_ACTIVE_PATH))
    {
        return {};
    }

    fs::path versionPath(PNOR_RO_ACTIVE_PATH);
    versionPath /= PNOR_VERSION_PARTITION;
    if (!fs::is_regular_file(versionPath))
    {
        return {};
    }

    std::ifstream versionFile(versionPath);
    std::string versionStr;
    std::getline(versionFile, versionStr);

    return versionStr;
}

bool MinimumShipLevel::verify()
{
    if (minShipLevel.empty())
    {
        return true;
    }

    auto actual = getFunctionalVersion();
    if (actual.empty())
    {
        return true;
    }

    Version minVersion = {0, 0, 0};
    parse(minShipLevel, minVersion);

    Version actualVersion = {0, 0, 0};
    parse(actual, actualVersion);

    auto rc = compare(actualVersion, minVersion);
    if (rc < 0)
    {
        log<level::ERR>(
            "PNOR Mininum Ship Level NOT met",
            entry("MIN_VERSION=%s", minShipLevel.c_str()),
            entry("ACTUAL_VERSION=%s", actual.c_str()),
            entry("VERSION_PURPOSE=%s",
                  "xyz.openbmc_project.Software.Version.VersionPurpose.Host"));
        return false;
    }

    return true;
}

} // namespace image
} // namespace software
} // namespace openpower
