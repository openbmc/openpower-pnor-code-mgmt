#include "image_verify.hpp"

#include <openssl/sha.h>

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

using namespace openpower::software::image;

class SignatureTest : public testing::Test
{
    static constexpr auto opensslCmd = "openssl dgst -sha256 -sign ";
    static constexpr auto testPath = "/tmp/_testSig";

  protected:
    void command(const std::string& cmd)
    {
        auto val = std::system(cmd.c_str());
        if (val)
        {
            std::cout << "COMMAND Error: " << val << std::endl;
        }
    }
    virtual void SetUp()
    {
        // Create test base directory.
        std::filesystem::create_directories(testPath);

        // Create unique temporary path for images.
        std::string tmpDir(testPath);
        tmpDir += "/extractXXXXXX";
        std::string imageDir = mkdtemp(const_cast<char*>(tmpDir.c_str()));

        // Create unique temporary configuration path
        std::string tmpConfDir(testPath);
        tmpConfDir += "/confXXXXXX";
        std::string confDir = mkdtemp(const_cast<char*>(tmpConfDir.c_str()));

        extractPath = imageDir;
        extractPath /= "images";

        signedConfPath = confDir;
        signedConfPath /= "conf";

        signedConfPNORPath = confDir;
        signedConfPNORPath /= "conf";
        signedConfPNORPath /= "OpenBMC";

        std::cout << "SETUP " << std::endl;

        command("mkdir " + extractPath.string());
        command("mkdir " + signedConfPath.string());
        command("mkdir " + signedConfPNORPath.string());

        std::string hashFile = signedConfPNORPath.string() + "/hashfunc";
        command("echo \"HashType=RSA-SHA256\" > " + hashFile);

        std::string manifestFile = extractPath.string() + "/" + "MANIFEST";
        command("echo \"HashType=RSA-SHA256\" > " + manifestFile);
        command("echo \"KeyType=OpenBMC\" >> " + manifestFile);

        std::string pnorFile = extractPath.string() + "/" + "pnor.xz.squashfs";
        command("echo \"pnor.xz.squashfs file \" > " + pnorFile);

        std::string pkeyFile = extractPath.string() + "/" + "private.pem";
        command("openssl genrsa  -out " + pkeyFile + " 4096");

        std::string pubkeyFile = extractPath.string() + "/" + "publickey";
        command("openssl rsa -in " + pkeyFile + " -outform PEM " +
                "-pubout -out " + pubkeyFile);

        std::string pubKeyConfFile = signedConfPNORPath.string() + "/" +
                                     "publickey";
        command("cp " + pubkeyFile + " " + signedConfPNORPath.string());
        command(opensslCmd + pkeyFile + " -out " + pnorFile + ".sig " +
                pnorFile);

        command(opensslCmd + pkeyFile + " -out " + manifestFile + ".sig " +
                manifestFile);
        command(opensslCmd + pkeyFile + " -out " + pubkeyFile + ".sig " +
                pubkeyFile);

        signature = std::make_unique<Signature>(extractPath, "pnor.xz.squashfs",
                                                signedConfPath);
    }
    virtual void TearDown()
    {
        command("rm -rf " + std::string(testPath));
    }
    std::unique_ptr<Signature> signature;
    std::filesystem::path extractPath;
    std::filesystem::path signedConfPath;
    std::filesystem::path signedConfPNORPath;
};

/** @brief Test for success scenario*/
TEST_F(SignatureTest, TestSignatureVerify)
{
    EXPECT_TRUE(signature->verify());
}

/** @brief Test failure scenario with corrupted signature file*/
TEST_F(SignatureTest, TestCorruptSignatureFile)
{
    // corrupt the image-kernel.sig file and ensure that verification fails
    std::string kernelFile = extractPath.string() + "/" + "pnor.xz.squashfs";
    command("echo \"dummy data\" > " + kernelFile + ".sig ");
    EXPECT_FALSE(signature->verify());
}

/** @brief Test failure scenario with no public key in the image*/
TEST_F(SignatureTest, TestNoPublicKeyInImage)
{
    // Remove publickey file from the image and ensure that verify fails
    std::string pubkeyFile = extractPath.string() + "/" + "publickey";
    command("rm " + pubkeyFile);
    EXPECT_FALSE(signature->verify());
}

/** @brief Test failure scenario with invalid hash function value*/
TEST_F(SignatureTest, TestInvalidHashValue)
{
    // Change the hashfunc value and ensure that verification fails
    std::string hashFile = signedConfPNORPath.string() + "/hashfunc";
    command("echo \"HashType=md5\" > " + hashFile);
    EXPECT_FALSE(signature->verify());
}

/** @brief Test for failure scenario with no config file in system*/
TEST_F(SignatureTest, TestNoConfigFileInSystem)
{
    // Remove the conf folder in the system and ensure that verify fails
    command("rm -rf " + signedConfPNORPath.string());
    EXPECT_FALSE(signature->verify());
}
