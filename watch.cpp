#include <stdexcept>
#include <cstddef>
#include <cstring>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>
#include <experimental/filesystem>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "watch.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;
using namespace std::string_literals;
namespace fs = std::experimental::filesystem;

Watch::Watch(sd_event* loop,
             std::function<void(std::string&)> functionalCallback) :
        functionalCallback(functionalCallback)
{
    // Create PNOR_ACTIVE_PATH if doesn't exist.
    if (!fs::is_directory(PNOR_ACTIVE_PATH))
    {
        fs::create_directories(PNOR_ACTIVE_PATH);
    }

    fd = inotify_init1(IN_NONBLOCK);
    if (-1 == fd)
    {
        // Store a copy of errno, because the string creation below will
        // invalidate errno due to one more system calls.
        auto error = errno;
        throw std::runtime_error(
                "inotify_init1 failed, errno="s + std::strerror(error));
    }

    wd = inotify_add_watch(fd, PNOR_ACTIVE_PATH, IN_CREATE);
    if (-1 == wd)
    {
        auto error = errno;
        close(fd);
        throw std::runtime_error(
                "inotify_add_watch failed, errno="s + std::strerror(error));
    }

    auto rc = sd_event_add_io(loop,
                              nullptr,
                              fd,
                              EPOLLIN,
                              callback,
                              this);
    if (0 > rc)
    {
        throw std::runtime_error(
                "failed to add to event loop, rc="s + std::strerror(-rc));
    }
}

Watch::~Watch()
{
    if ((-1 != fd) && (-1 != wd))
    {
        inotify_rm_watch(fd, wd);
        close(fd);
    }
}

int Watch::callback(sd_event_source* s,
                    int fd,
                    uint32_t revents,
                    void* userdata)
{
    if (!(revents & EPOLLIN))
    {
        return 0;
    }

    constexpr auto maxBytes = 1024;
    uint8_t buffer[maxBytes];
    auto bytes = read(fd, buffer, maxBytes);
    if (0 > bytes)
    {
        auto error = errno;
        throw std::runtime_error(
                "failed to read inotify event, errno="s + std::strerror(error));
    }

    auto offset = 0;
    while (offset < bytes)
    {
        auto event = reinterpret_cast<inotify_event*>(&buffer[offset]);
        // Update the functional association on a RO
        // active image symlink change
        auto path = std::string{PNOR_ACTIVE_PATH} + event->name;
        if (path.compare(PNOR_RO_ACTIVE_PATH) == 0)
        {
            // Read the symlink target
            char buf[1024];
            int len;
            if ((len = readlink(path.c_str(), buf, sizeof(buf) - 1)) != -1)
            {
                buf[len] = '\0';
            }
            auto target = std::string(buf);

            // Get the image <id> from the symlink target
            // for example /media/ro-2a1022fe
            static const auto PNOR_RO_PREFIX_LEN = strlen(PNOR_RO_PREFIX);
            auto id = target.substr(PNOR_RO_PREFIX_LEN);
            auto objPath = std::string{SOFTWARE_OBJPATH} + '/' + id;

            static_cast<Watch*>(userdata)->functionalCallback(objPath);
        }
        offset += offsetof(inotify_event, name) + event->len;
    }

    return 0;
}

} // namespace updater
} // namespace software
} // namespace openpower
