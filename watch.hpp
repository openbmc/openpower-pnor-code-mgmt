#pragma once

#include <systemd/sd-event.h>
#include <unistd.h>

#include <functional>
#include <memory>

namespace openpower
{
namespace software
{
namespace updater
{

/* Need a custom deleter for freeing up sd_event_source */
struct EventSourceDeleter
{
    void operator()(sd_event_source* eventSource) const
    {
        eventSource = sd_event_source_unref(eventSource);
    }
};
using EventSourcePtr = std::unique_ptr<sd_event_source, EventSourceDeleter>;

/** @struct CustomFd
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFd
{
  public:
    CustomFd() = delete;
    CustomFd(const CustomFd&) = delete;
    CustomFd& operator=(const CustomFd&) = delete;
    CustomFd(CustomFd&&) = delete;
    CustomFd& operator=(CustomFd&&) = delete;

    /** @brief Saves File descriptor and uses it to do file operation
     *
     *  @param[in] fd - File descriptor
     */
    CustomFd(int fd) : fd(fd)
    {
    }

    ~CustomFd()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    /** @brief File descriptor */
    int fd = -1;
};

/** @class Watch
 *
 *  @brief Adds inotify watch on PNOR symlinks file to monitor for changes in
 *         "running" PNOR version
 *
 *  The inotify watch is hooked up with sd-event, so that on call back,
 *  appropriate actions related to a change in the "running" PNOR version
 *  can be taken.
 */
class Watch
{
  public:
    /** @brief ctor - hook inotify watch with sd-event
     *
     *  @param[in] loop - sd-event object
     *  @param[in] functionalCallback - The callback function for updating
     *                                  the functional associations.
     */
    Watch(sd_event* loop, std::function<void(std::string&)> functionalCallback);

    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&&) = delete;
    Watch& operator=(Watch&&) = delete;

    /** @brief dtor - remove inotify watch
     */
    ~Watch();

  private:
    /** @brief sd-event callback
     *
     *  @param[in] s - event source, floating (unused) in our case
     *  @param[in] fd - inotify fd
     *  @param[in] revents - events that matched for fd
     *  @param[in] userdata - pointer to Watch object
     *  @returns 0 on success, -1 on fail
     */
    static int callback(sd_event_source* s, int fd, uint32_t revents,
                        void* userdata);

    /**  initialize an inotify instance and returns file descriptor */
    int inotifyInit();

    /** @brief PNOR symlink file watch descriptor */
    int wd = -1;

    /** @brief event source */
    EventSourcePtr eventSource;

    /** @brief The callback function for updating the
               functional associations. */
    std::function<void(std::string&)> functionalCallback;

    /** @brief inotify file descriptor */
    CustomFd fd;
};

} // namespace updater
} // namespace software
} // namespace openpower
