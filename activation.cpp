#include <experimental/filesystem>
#include <fstream>
#include <regex>
#include "activation.hpp"
#include "config.h"

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

int Activation::writePartitions()
{
    constexpr auto rwFlag = "READWRITE";
    std::string line;
    std::smatch match;

    // Partition line format: partitionXX=name,startAddr,endAddr,flag,[flag,..]
    std::regex regex
    {
        "^partition([0-9]+)=([A-Za-z0-9_]+),",
        std::regex::extended
    };

    fs::path toc = PNOR_RO_PREFIX;
    toc += versionId;
    toc /= PNOR_TOC_FILE;
    std::ifstream file(toc.c_str());

    while (std::getline(file, line))
    {
        if (std::regex_search(line, match, regex))
        {
            auto name = match[2].str();
            auto suffix = match.suffix().str();

            if (std::string::npos != suffix.find(rwFlag))
            {
                fs::path source = PNOR_RO_PREFIX;
                source += versionId;
                source /= name;
                fs::path target = PNOR_RW_PREFIX;
                target += versionId;
                target /= name;
                fs::copy_file(source, target,
                        fs::copy_options::overwrite_existing);
            }
        }
    }

    return 0;
}

auto Activation::activation(Activations value) ->
        Activations
{
    if (value == softwareServer::Activation::Activations::Activating)
    {
        if (!activationBlocksTransition)
        {
            activationBlocksTransition =
                      std::make_unique<ActivationBlocksTransition>(
                                bus,
                                path);
        }

        // Creating a mount point to access squashfs image.
        constexpr auto squashfsMountService = "obmc-flash-bios-squashfsmount@";
        auto squashfsMountServiceFile = std::string(squashfsMountService) +
                                                    versionId + ".service";
        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(squashfsMountServiceFile,
                      "replace");
        bus.call_noreply(method);
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    if ((value == softwareServer::Activation::RequestedActivations::Active) &&
        (softwareServer::Activation::requestedActivation() !=
                  softwareServer::Activation::RequestedActivations::Active))
    {
        constexpr auto ubimountService = "obmc-flash-bios-ubimount@";
        auto ubimountServiceFile = std::string(ubimountService) +
                                   versionId +
                                   ".service";
        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(ubimountServiceFile,
                      "replace");
        bus.call_noreply(method);

        auto rc = writePartitions();
        if (rc == 0)
        {
            fs::create_directories(PNOR_ACTIVE_PATH);

            std::string target(PNOR_RO_PREFIX + versionId);
            fs::create_directory_symlink(target, PNOR_RO_ACTIVE_PATH);

            target = PNOR_RW_PREFIX + versionId;
            fs::create_directory_symlink(target, PNOR_RW_ACTIVE_PATH);
        }
        else
        {
            Activation::activation(
                    softwareServer::Activation::Activations::Failed);
        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

