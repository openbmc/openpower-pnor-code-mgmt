#include "config.h"
#include <experimental/filesystem>
#include <cereal/archives/binary.hpp>
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
    if(!fs::is_directory(SERIAL_DIR))
    {
        fs::create_directory(SERIAL_DIR);
    }
    std::string path = SERIAL_DIR + versionId;

    std::ofstream os(path.c_str(), std::ios::binary);
    cereal::BinaryOutputArchive oarchive(os);
    oarchive(priority);
}

void restoreFromFile(std::string versionId, uint8_t *priority)
{
    std::string path = SERIAL_DIR + versionId;
    if (fs::exists(path))
    {
        std::ifstream is(path.c_str(), std::ios::in | std::ios::binary);
        cereal::BinaryInputArchive iarchive(is);
        iarchive(*priority);
    }
}

void removeFile(std::string versionId)
{
    std::string path = SERIAL_DIR + versionId;
    if (fs::exists(path))
    {
        fs::remove(path);
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
