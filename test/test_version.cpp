#include "version.hpp"

#include <openssl/sha.h>

#include <gtest/gtest.h>

using namespace openpower::software::updater;

/** @brief Make sure we correctly get the Id from getId()*/
TEST(VersionTest, TestGetId)
{
    auto version = "test-id";
    unsigned char digest[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    SHA512_Init(&ctx);
    SHA512_Update(&ctx, version, strlen(version));
    SHA512_Final(digest, &ctx);
    char mdString[SHA512_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i * 2], 3, "%02x", (unsigned int)digest[i]);
    }
    std::string hexId = std::string(mdString);
    hexId = hexId.substr(0, 8);
    EXPECT_EQ(Version::getId(version), hexId);
}

TEST(VersionTest, GetVersions)
{
    constexpr auto versionString =
        "open-power-romulus-v2.2-rc1-48-g268344f-dirty\n"
        "\tbuildroot-2018.11.1-7-g5d7cc8c\n"
        "\tskiboot-v6.2\n"
        "\thostboot-3f1f218-pea87ca7\n"
        "\tocc-12c8088\n"
        "\tlinux-4.19.13-openpower1-p8031295\n"
        "\tpetitboot-1.9.2\n"
        "\tmachine-xml-7410460\n"
        "\thostboot-binaries-hw121518a.930\n"
        "\tcapp-ucode-p9-dd2-v4\n"
        "\tsbe-cf61dc3\n"
        "\thcode-hw123119a.930";

    const auto& [version, extendedVersion] =
        Version::getVersions(versionString);
    EXPECT_EQ(version, "open-power-romulus-v2.2-rc1-48-g268344f-dirty");
    EXPECT_EQ(extendedVersion, "buildroot-2018.11.1-7-g5d7cc8c,"
                               "skiboot-v6.2,"
                               "hostboot-3f1f218-pea87ca7,"
                               "occ-12c8088,"
                               "linux-4.19.13-openpower1-p8031295,"
                               "petitboot-1.9.2,"
                               "machine-xml-7410460,"
                               "hostboot-binaries-hw121518a.930,"
                               "capp-ucode-p9-dd2-v4,"
                               "sbe-cf61dc3,"
                               "hcode-hw123119a.930");
}
