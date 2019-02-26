#pragma once

#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace utils
{

using namespace phosphor::logging;

template <typename... Ts>
std::string concat_string(Ts const&... ts)
{
    std::stringstream s;
    ((s << ts << " "), ...) << std::endl;
    return s.str();
}

// Helper function to run pflash command
// Returns return code and the stdout
template <typename... Ts>
std::pair<int, std::string> pflash(Ts const&... ts)
{
    std::array<char, 512> buffer;
    std::string cmd = concat_string("pflash", ts...);
    std::stringstream result;
    int rc;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result << buffer.data();
    }
    rc = pclose(pipe);
    return {rc, result.str()};
}

inline std::string getPNORVersion()
{
    // Read the VERSION partition skipping the first 4K
    auto r = pflash("-P", "VERSION", "-r", "/dev/stderr", "--skip=4096",
                    "2>&1 > /dev/null");
    return r.second;
}

inline void pnorClear(const std::string& part, bool shouldEcc = true)
{
    int rc;
    std::tie(rc, std::ignore) =
        utils::pflash("-P", part, shouldEcc ? "-c" : "-e", "-f >/dev/null");
    if (rc != 0)
    {
        log<level::ERR>("Failed to clear partition",
                        entry("PART=%s", part.c_str()),
                        entry("RETURNCODE=%d", rc));
    }
    else
    {
        log<level::INFO>("Clear partition successfully",
                         entry("PART=%s", part.c_str()));
    }
}

// The pair contains the partition name and if it should use ECC clear
using PartClear = std::pair<std::string, bool>;

inline std::vector<PartClear> getPartsToClear(const std::string& info)
{
    std::vector<PartClear> ret;
    std::istringstream iss(info);
    std::string line;

    while (std::getline(iss, line))
    {
        // Each line looks like
        // ID=06 MVPD 0x0012d000..0x001bd000 (actual=0x00090000) [E--P--F-C-]
        // Flag 'F' means REPROVISION
        // Flag 'C' means CLEARECC
        auto flags = line.substr(line.find('['));
        if (flags.find('F') != std::string::npos)
        {
            // This is a partition to be cleared
            line = line.substr(line.find_first_of(' '));     // Skiping "ID=xx"
            line = line.substr(line.find_first_not_of(' ')); // Skipping spaces
            line = line.substr(0, line.find_first_of(' '));  // The part name
            bool ecc = flags.find('C') != std::string::npos;
            ret.emplace_back(line, ecc);
        }
    }
    return ret;
}

// Get partitions that should be cleared
inline std::vector<PartClear> getPartsToClear()
{
    const auto& [rc, pflashInfo] = pflash("-i | grep ^ID | grep 'F'");
    return getPartsToClear(pflashInfo);
}

} // namespace utils
