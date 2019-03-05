#include "config.h"

#include "serialize.hpp"

#include <cereal/archives/json.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <sdbusplus/server.hpp>

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

void storeToFile(const std::string& versionId, uint8_t priority)
{
    auto bus = sdbusplus::bus::new_default();

    if (!fs::is_directory(PERSIST_DIR))
    {
        fs::create_directories(PERSIST_DIR);
    }

    // store one copy in /var/lib/obmc/openpower-pnor-code-mgmt/[versionId]
    auto varPath = PERSIST_DIR + versionId;
    std::ofstream varOutput(varPath.c_str());
    cereal::JSONOutputArchive varArchive(varOutput);
    varArchive(cereal::make_nvp("priority", priority));

    if (fs::is_directory(PNOR_RW_PREFIX + versionId))
    {
        // store another copy in /media/pnor-rw-[versionId]/[versionId]
        auto rwPath = PNOR_RW_PREFIX + versionId + "/" + versionId;
        std::ofstream rwOutput(rwPath.c_str());
        cereal::JSONOutputArchive rwArchive(rwOutput);
        rwArchive(cereal::make_nvp("priority", priority));
    }

    // lastly, store the priority as an environment variable pnor-[versionId]
    std::string serviceFile = "obmc-flash-bmc-setenv@pnor\\x2d" + versionId +
                              "\\x3d" + std::to_string(priority) + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

bool restoreFromFile(const std::string& versionId, uint8_t& priority)
{
    auto varPath = PERSIST_DIR + versionId;
    if (fs::exists(varPath))
    {
        std::ifstream varInput(varPath.c_str(), std::ios::in);
        try
        {
            cereal::JSONInputArchive varArchive(varInput);
            varArchive(cereal::make_nvp("priority", priority));
            return true;
        }
        catch (cereal::RapidJSONException& e)
        {
            fs::remove(varPath);
        }
    }

    auto rwPath = PNOR_RW_PREFIX + versionId + "/" + versionId;
    if (fs::exists(rwPath))
    {
        std::ifstream rwInput(rwPath.c_str(), std::ios::in);
        try
        {
            cereal::JSONInputArchive rwArchive(rwInput);
            rwArchive(cereal::make_nvp("priority", priority));
            return true;
        }
        catch (cereal::RapidJSONException& e)
        {
            fs::remove(rwPath);
        }
    }

    try
    {
        std::string devicePath = "/dev/mtd/u-boot-env";

        if (fs::exists(devicePath) && !devicePath.empty())
        {
            std::ifstream input(devicePath.c_str());
            std::string envVars;
            std::getline(input, envVars);

            std::string versionVar = "pnor-" + versionId + "=";
            auto varPosition = envVars.find(versionVar);

            if (varPosition != std::string::npos)
            {
                // Grab the environment variable for this versionId. These
                // variables follow the format "pnor-[versionId]=[priority]\0"
                auto var = envVars.substr(varPosition);
                priority = std::stoi(var.substr(versionVar.length()));
                return true;
            }
        }
    }
    catch (const std::exception& e)
    {
    }

    return false;
}

void removeFile(const std::string& versionId)
{
    auto bus = sdbusplus::bus::new_default();

    // Clear the environment variable pnor-[versionId].
    std::string serviceFile =
        "obmc-flash-bmc-setenv@pnor\\x2d" + versionId + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);

    // Delete the file /var/lib/obmc/openpower-pnor-code-mgmt/[versionId].
    // Note that removeFile() is called in the case of a version being deleted,
    // so the file /media/pnor-rw-[versionId]/[versionId] will also be deleted
    // along with its surrounding directory.
    std::string path = PERSIST_DIR + versionId;
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
