#pragma once

#include <systemd/sd-event.h>

namespace openpower
{
namespace software
{
namespace updater
{

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
        Watch(sd_event* loop,
              std::function<void(std::string&)> functionalCallback);

        Watch(const Watch&) = delete;
        Watch& operator=(const Watch&) = delete;
        Watch(Watch&&) = default;
        Watch& operator=(Watch&&) = default;

        /** @brief dtor - remove inotify watch and close fd's
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
        static int callback(sd_event_source* s,
                            int fd,
                            uint32_t revents,
                            void* userdata);

        /** @brief PNOR symlink file watch descriptor */
        int wd = -1;

        /** @brief inotify file descriptor */
        int fd = -1;

        /** @brief The callback function for updating the
                   functional associations. */
        std::function<void(std::string&)> functionalCallback;
};

} // namespace updater
} // namespace software
} // namespace openpower
