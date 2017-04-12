#include <sys/wait.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

constexpr auto PNOR_SQUASHFS_IMAGE = "pnor.xz.squashfs";
constexpr auto IMAGES_DIR = "/tmp/images/";

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{
    auto* updater = static_cast<ItemUpdater*>(userData);
    auto mapper = updater->busItem.new_method_call(
            MAPPER_BUSNAME,
            MAPPER_PATH,
            MAPPER_INTERFACE,
            "GetSubTreePaths");
    mapper.append(SOFTWARE_OBJPATH,
                  1, // Depth
                  std::vector<std::string>({VERSION_IFACE}));

    auto mapperResponseMsg = updater->busItem.call(mapper);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return -1;
    }

    std::vector<std::string> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return -1;
    }

    for (const auto& resp : mapperResponse)
    {
        // Version id is the last item in the path
        auto pos = resp.rfind("/");
        if (pos == std::string::npos)
        {
            log<level::ERR>("No version id found in object path",
                    entry("OBJPATH=%s", resp));
            return -1;
        }

        auto versionId = resp.substr(pos + 1);

        // Determine the Activation state by processing the given tarball.
        auto activationState = server::Activation::Activations::Invalid;
        if (ItemUpdater::processImage(TARBALL_PATH, versionId) == 0)
        {
            activationState = server::Activation::Activations::Ready;
        }

        if (updater->activations.find(versionId) == updater->activations.end())
        {
            updater->activations.insert(std::make_pair(
                    versionId,
                    std::make_unique<Activation>(
                            updater->busItem,
                            resp,
                            activationState)));
        }
    }
    return 0;
}

int ItemUpdater::processImage(const std::string& tarballFilePath,
                              std::string& versionId)
{
    if (ItemUpdater::validateTarball(tarballFilePath) != 0)
    {
        log<level::ERR>("Failed to validate the tarball.");
        return -1;
    }
    if (ItemUpdater::unTAR(tarballFilePath, versionId) != 0)
    {
        log<level::ERR>("Failed to untar the tarball.");
        return -1;
    }

    return 0;
}

int ItemUpdater::createFolder(std::string& versionId)
{
    pid_t pid = fork();
    if(pid < 0)
    {
        log<level::ERR>("fork() failed.");
        return -1;
    }
    if(pid != 0)
    {
        wait(NULL); // wait for child process to join with parent
    }
    else
    {
        execl("/bin/mkdir", "mkdir", "-p", (std::string(IMAGES_DIR)
              + versionId).c_str(), (char *)0);
        // execl only returns on fail
        return -1;
    }
    return 0;
}

int ItemUpdater::unTAR(const std::string& tarballFilePath,
                       std::string& versionId)
{
    if(ItemUpdater::createFolder(versionId) != 0)
    {
        log<level::ERR>("Failed to create folder in",
                        entry("FILENAME=%s", (std::string(IMAGES_DIR))));
        return -1;
    }

    pid_t pid = fork();
    if(pid < 0)
    {
        log<level::ERR>("fork() failed.");
        return -1;
    }
    if(pid != 0)
    {
        wait(NULL); // wait for child process to join with parent
    }
    else
    {
        execl("/bin/tar", "tar", "-xvf", tarballFilePath.c_str(),
              "-C", (std::string(IMAGES_DIR) + versionId).c_str(),
              (char *)0);

        // execl only returns on fail
        log<level::ERR>("Failed to untar file",
                        entry("FILENAME=%s", tarballFilePath));
        return -1;
    }
    return 0;
}


int ItemUpdater::validateTarball(const std::string& tarballFilePath)
{
    int link[2];
    pid_t pid;
    char output[4096];

    if (pipe(link)==-1)
    {
        log<level::ERR>("Pipe failed.");
        return -1;
    }

    if ((pid = fork()) < 0)
    {
        log<level::ERR>("fork() failed.");
        return -1;
    }
    if(pid == 0)
    {
        // Child Process.
        dup2 (link[1], STDOUT_FILENO);
        close(link[0]);
        close(link[1]);
        execl("/bin/tar", "tar", "-t", "-f", tarballFilePath.c_str(), (char *)0);
        return -1;
    }
    else
    {
        // Parent Process.
        close(link[1]);
        link[0] = read(link[0], output, sizeof(output));
        std::string str(output);
        if(str.find(PNOR_SQUASHFS_IMAGE) != std::string::npos)
        {
            // Found squashfs image in the tarball
            return 0;
        }
        else
        {
            log<level::ERR>("Unable to find the squashfs image in tarball.");
            return -1;
        }
        wait(NULL);
    }
    return -1;
}


} // namespace manager
} // namespace software
} // namespace openpower

