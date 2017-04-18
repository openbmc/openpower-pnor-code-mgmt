#include <algorithm>
#include <experimental/filesystem>
#include <fstream>
#include <sstream>
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

int Activation::writePartition()
{
    constexpr auto partitionKey = "partition";
    constexpr auto rwFlag = "READWRITE";
    constexpr auto rwFlagSize = strlen(rwFlag);

    std::ifstream toc("/media/pnor-ro-" + versionId + "/pnor.toc");
    toc.exceptions(std::ifstream::failbit
                     | std::ifstream::badbit
                     | std::ifstream::eofbit);
    std::string line;

    try
    {
        if (!toc.is_open())
        {
            return -1;
        }
        while (std::getline(toc, line))
        {
            // Partition line format: partitionXX=value1,value2,...
            auto pos = line.find(partitionKey);
            if (pos == std::string::npos)
            {
                continue;
            }
            // Replace the ',' delimiter with spaces for parsing
            std::replace(line.begin(), line.end(), ',', ' ');
            pos = line.find("=");
            if (pos == std::string::npos)
            {
                return -1;
            }
            std::istringstream data(line.substr(pos + 1));
            std::string name, startAddr, endAddr, flag;
            // Partitions can have one or more flags
            if (data >> name >> startAddr >> endAddr >> flag)
            {
                do
                {
                    // Only write the partitions with RW flag
                    if (flag.compare(0, flag.size(), rwFlag, rwFlagSize) == 0)
                    {
                        fs::path source = "/media/pnor-ro-";
                        source += versionId;
                        source /= name;
                        fs::path target = "/media/pnor-rw-";
                        target += versionId;
                        target /= name;
                        fs::copy_file(source, target,
                                fs::copy_options::overwrite_existing);
                    }
                } while(data >> flag);
            }
            else
            {
                return -1;
            }
        }
        toc.close();
    }
    catch (const std::exception& e)
    {
        if (toc.eof())
        {
            // getline() reads beyond last line and sets the error bit,
            // clear if that's the case.
            toc.clear();
            toc.close();
            return 0;
        }
        return -1;
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
        auto rc = writePartition();
        if (rc == 0)
        {
            std::string target("/media/pnor-ro-" + versionId);
            std::experimental::filesystem::create_directory_symlink(
                    target, ACTIVE_PNOR_RO);
            target = "/media/pnor-rw-" + versionId;
            std::experimental::filesystem::create_directory_symlink(
                    target, ACTIVE_PNOR_RW);
        }
        else
        {
            // Handle error
        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

