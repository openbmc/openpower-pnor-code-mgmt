#include "config.h"

#include "watch.hpp"

#include "item_updater.hpp"

#include <sys/inotify.h>
#include <unistd.h>

#include <cstddef>
#include <cstring>
#include <experimental/filesystem>
#include <functional>
#include <phosphor-logging/log.hpp>
#include <stdexcept>
#include <string>

namespace openpower
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;
namespace fs = std::experimental::filesystem;

Watch::Watch(sd_event* loop,
             std::function<void(std::string&)> functionalCallback) :
    functionalCallback(functionalCallback),
    fd(inotifyInit())

{
    // Create PNOR_ACTIVE_PATH if doesn't exist.
    if (!fs::is_directory(PNOR_ACTIVE_PATH))
    {
        fs::create_directories(PNOR_ACTIVE_PATH);
    }

    wd = inotify_add_watch(fd(), PNOR_ACTIVE_PATH, IN_CREATE);
    if (-1 == wd)
    {
        auto error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Error occurred during the inotify_init1");
    }

    decltype(eventSource.get()) sourcePtr = nullptr;
    auto rc = sd_event_add_io(loop, &sourcePtr, fd(), EPOLLIN, callback, this);

    eventSource.reset(sourcePtr);

    if (0 > rc)
    {
        throw std::system_error(-rc, std::generic_category(),
                                "Error occurred during the inotify_init1");
    }
}

Watch::~Watch()
{
    if ((-1 != fd()) && (-1 != wd))
    {
        inotify_rm_watch(fd(), wd);
    }
}

int Watch::callback(sd_event_source* s, int fd, uint32_t revents,
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
        throw std::system_error(error, std::generic_category(),
                                "failed to read inotify event");
    }

    auto offset = 0;
    while (offset < bytes)
    {
        auto event = reinterpret_cast<inotify_event*>(&buffer[offset]);
        // Update the functional association on a RO
        // active image symlink change
        fs::path path(PNOR_ACTIVE_PATH);
        path /= event->name;
        if (fs::equivalent(path, PNOR_RO_ACTIVE_PATH))
        {
            auto id = ItemUpdater::determineId(path);
            auto objPath = std::string{SOFTWARE_OBJPATH} + '/' + id;

            static_cast<Watch*>(userdata)->functionalCallback(objPath);
        }
        offset += offsetof(inotify_event, name) + event->len;
    }

    return 0;
}

int Watch::inotifyInit()
{
    auto fd = inotify_init1(IN_NONBLOCK);

    if (-1 == fd)
    {
        auto error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Error occurred during the inotify_init1");
    }

    return fd;
}

} // namespace updater
} // namespace software
} // namespace openpower
