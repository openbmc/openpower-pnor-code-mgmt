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

} // namespace utils
