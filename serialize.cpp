#include "config.h"
#include <experimental/filesystem>
#include <cereal/archives/json.hpp>
#include <fstream>
#include "serialize.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;

void storeToFile(std::string versionId, uint8_t priority)
{
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
}

bool restoreFromFile(std::string versionId, uint8_t& priority)
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
    return false;
}

void removeFile(std::string versionId)
{
    std::string path = PERSIST_DIR + versionId;
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
