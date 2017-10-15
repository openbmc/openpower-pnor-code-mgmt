#include <gtest/gtest.h>
#include <openssl/sha.h>
#include <string>
#include "version.hpp"

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
    char mdString[SHA512_DIGEST_LENGTH*2+1];
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i*2], 3, "%02x", (unsigned int)digest[i]);
    }
    std::string hexId = std::string(mdString);
    hexId = hexId.substr(0, 8);
    EXPECT_EQ(Version::getId(version), hexId);
}
