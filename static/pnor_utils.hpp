#pragma once

#include <sstream>
#include <string>

namespace utils
{

template <typename... Ts>
std::string concat_string(Ts const&... ts)
{
    std::stringstream s;
    ((s << ts << " "), ...) << std::endl;
    return s.str();
}

// Helper function to run pflash command
template <typename... Ts>
std::string pflash(Ts const&... ts)
{
    std::array<char, 512> buffer;
    std::string cmd = concat_string("pflash", ts...);
    std::stringstream result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result << buffer.data();
    }
    return result.str();
}

inline std::string getPNORVersion()
{
    // Read the VERSION partition skipping the first 4K
    auto r = pflash("-P", "VERSION", "-r", "/dev/stderr", "--skip=4096",
                    "2>&1 > /dev/null");
    return r;
}

} // namespace utils
