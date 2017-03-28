#include "version_host_software_manager.hpp"
#include <gtest/gtest.h>
#include <experimental/filesystem>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace openpower::software::manager;
namespace fs = std::experimental::filesystem;


class VersionTest : public testing::Test
{
    protected:

        virtual void SetUp()
        {
            char versionDir[20] = "/tmp/versionXXXXXX";
            auto path = mkdtemp(versionDir);

            _directory = path;
        }

        virtual void TearDown()
        {
            fs::remove_all(_directory);
        }

        std::string _directory;
};

/** @brief Make sure we correctly get the version from getVersion()*/
TEST_F(VersionTest, TestGetVersion)
{
    auto tocFilePath = _directory + "/" + "pnor.toc";
    auto version = "test-version";

    std::ofstream file;
    file.open(tocFilePath, std::ofstream::out);
    assert(file.is_open());

    file << "version=" << version << std::endl;
    file.close();

    EXPECT_EQ(Version::getVersion(tocFilePath),version);
}

/** @brief Make sure we correctly get the Id from getId()*/
TEST_F(VersionTest, TestGetId)
{
    std::stringstream hexId;
    auto version = "test-id";

    hexId << std::hex << ((std::hash<std::string> {}(
                               version)) & 0xFFFFFFFF);

    EXPECT_EQ(Version::getId(version),hexId.str());

}
