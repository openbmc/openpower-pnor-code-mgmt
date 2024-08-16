#include "version.hpp"

#include <openssl/evp.h>

#include <gtest/gtest.h>

using namespace openpower::software::updater;

using EVP_MD_CTX_Ptr =
    std::unique_ptr<EVP_MD_CTX, decltype(&::EVP_MD_CTX_free)>;

/** @brief Make sure we correctly get the Id from getId()*/
TEST(VersionTest, TestGetId)
{
    auto version = "test-id";
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_count = 0;

    EVP_MD_CTX_Ptr ctx(EVP_MD_CTX_new(), &::EVP_MD_CTX_free);

    EVP_DigestInit(ctx.get(), EVP_sha512());
    EVP_DigestUpdate(ctx.get(), version, strlen(version));
    EVP_DigestFinal(ctx.get(), digest, &digest_count);

    char mdString[EVP_MAX_MD_SIZE * 2 + 1];
    for (decltype(digest_count) i = 0; i < digest_count; i++)
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
    EXPECT_EQ(extendedVersion,
              "buildroot-2018.11.1-7-g5d7cc8c,"
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
