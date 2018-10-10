#include "config.h"

#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <regex>
#include <xyz/openbmc_project/Control/MinimumShipLevel/error.hpp>

namespace openpower
{
namespace software
{
namespace msl
{

namespace fs = std::experimental::filesystem;
using namespace phosphor::logging;

struct Version_t
{
    uint32_t major;
    uint32_t minor;
    uint32_t rev;
};

/**
 * @brief Compare the versions provided
 * @param[in] a - The first version to compare
 * @param[in] b - The second version to compare
 * @return 0 if a <= b, -1 otherwise.
 */
static int compare(const Version_t& a, const Version_t& b)
{
    if (a.major > b.major)
    {
        return -1;
    }
    else if (a.major < b.major)
    {
        return 0;
    }

    if (a.minor > b.minor)
    {
        return -1;
    }
    else if (a.minor < b.minor)
    {
        return 0;
    }

    if (a.rev > b.rev)
    {
        return -1;
    }
    else if (a.rev < b.rev)
    {
        return 0;
    }

    return 0;
}

/**
 * @brief Parse the version parts into a struct
 * @details Version format follows a git tag convention: vX.Y[.Z]
 *          Reference:
 *          https://github.com/open-power/op-build/blob/master/openpower/package/VERSION.readme
 * @param[in] version - The version string to be parsed
 * @param[out] version_t - The version struct to be populated
 */
static void parse(const std::string& version, Version_t& version_t)
{
    std::smatch match;

    // Match for vX.Y.Z
    std::regex regex{"v([0-9]+)\\.([0-9]+)\\.([0-9]+)", std::regex::extended};

    if (!std::regex_search(version, match, regex))
    {
        // Match for vX.Y
        std::regex regexShort{"v([0-9]+)\\.([0-9]+)", std::regex::extended};
        if (!std::regex_search(version, match, regexShort))
        {
            log<level::ERR>("Unable to parse PNOR version",
                            entry("VERSION=%s", version.c_str()));
            return;
        }
    }
    else
    {
        // Populate Z
        version_t.rev = std::stoi(match[3]);
    }
    version_t.major = std::stoi(match[1]);
    version_t.minor = std::stoi(match[2]);
}

static std::string getFunctionalPnorVersion()
{
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

/**
 * @brief Verify that the current PNOR version meets the min ship level (msl)
 */
static void verify()
{
    using namespace sdbusplus::xyz::openbmc_project::Control::MinimumShipLevel::
        Error;
    using notMet = xyz::openbmc_project::Control::MinimumShipLevel::NotMet;

    constexpr auto purpose =
        "xyz.openbmc_project.Software.Version.VersionPurpose.Host";
    auto min = std::string{PNOR_MSL};

    if (std::string(PNOR_MSL).empty())
    {
        return;
    }

    if (!fs::exists(PNOR_RO_ACTIVE_PATH))
    {
        return;
    }

    auto actual = getFunctionalPnorVersion();
    if (actual.empty())
    {
        return;
    }

    Version_t min_t = {0, 0, 0};
    parse(min, min_t);

    Version_t actual_t = {0, 0, 0};
    parse(actual, actual_t);

    auto rc = compare(min_t, actual_t);
    if (rc != 0)
    {
        log<level::ERR>("PNOR Mininum Ship Level NOT met",
                        entry("MIN_VERSION=%s", min.c_str()),
                        entry("ACTUAL_VERSION=%s", actual.c_str()),
                        entry("VERSION_PURPOSE=%s", purpose));
        report<NotMet>(notMet::MIN_VERSION(min.c_str()),
                       notMet::ACTUAL_VERSION(actual.c_str()),
                       notMet::VERSION_PURPOSE(purpose));
    }

    return;
}
} // namespace msl
} // namespace software
} // namespace openpower

int main(int argc, char** argv)
{
    openpower::software::msl::verify();

    return 0;
}
