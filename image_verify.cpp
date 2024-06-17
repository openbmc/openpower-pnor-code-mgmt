#include "config.h"

#include "image_verify.hpp"

#include "version.hpp"

#include <fcntl.h>
#include <openssl/err.h>
#include <sys/stat.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <fstream>
#include <set>

namespace openpower
{
namespace software
{
namespace image
{

using namespace phosphor::logging;
using namespace openpower::software::updater;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

constexpr auto keyTypeTag = "KeyType";
constexpr auto hashFunctionTag = "HashType";

Signature::Signature(const std::filesystem::path& imageDirPath,
                     const std::string& pnorFileName,
                     const std::filesystem::path& signedConfPath) :
    imageDirPath(imageDirPath),
    pnorFileName(pnorFileName), signedConfPath(signedConfPath)
{
    std::filesystem::path file(imageDirPath / MANIFEST_FILE);

    auto keyValues =
        Version::getValue(file, {{keyTypeTag, " "}, {hashFunctionTag, " "}});
    keyType = keyValues.at(keyTypeTag);
    hashType = keyValues.at(hashFunctionTag);
}

AvailableKeyTypes Signature::getAvailableKeyTypesFromSystem() const
{
    AvailableKeyTypes keyTypes{};

    // Find the path of all the files
    if (!std::filesystem::is_directory(signedConfPath))
    {
        log<level::ERR>("Signed configuration path not found in the system");
        elog<InternalFailure>();
    }

    // Look for all the hash and public key file names get the key value
    // For example:
    // /etc/activationdata/OpenPOWER/publickey
    // /etc/activationdata/OpenPOWER/hashfunc
    // /etc/activationdata/GA/publickey
    // /etc/activationdata/GA/hashfunc
    // Set will have OpenPOWER, GA

    for (const auto& p :
         std::filesystem::recursive_directory_iterator(signedConfPath))
    {
        if ((p.path().filename() == HASH_FILE_NAME) ||
            (p.path().filename() == PUBLICKEY_FILE_NAME))
        {
            // extract the key types
            // /etc/activationdata/GA/  -> get GA from the path
            auto key = p.path().parent_path();
            keyTypes.insert(key.filename());
        }
    }

    return keyTypes;
}

inline KeyHashPathPair Signature::getKeyHashFileNames(const Key_t& key) const
{
    std::filesystem::path hashpath(signedConfPath / key / HASH_FILE_NAME);
    std::filesystem::path keyPath(signedConfPath / key / PUBLICKEY_FILE_NAME);

    return std::make_pair(std::move(hashpath), std::move(keyPath));
}

bool Signature::verify()
{
    try
    {
        // Verify the MANIFEST and publickey file using available
        // public keys and hash on the system.
        if (false == systemLevelVerify())
        {
            log<level::ERR>("System level Signature Validation failed");
            return false;
        }

        // image specific publickey file name.
        std::filesystem::path publicKeyFile(imageDirPath / PUBLICKEY_FILE_NAME);

        // Validate the PNOR image file.
        // Build Image File name
        std::filesystem::path file(imageDirPath);
        file /= pnorFileName;

        // Build Signature File name
        std::string fileName = file.filename();
        std::filesystem::path sigFile(imageDirPath);
        sigFile /= fileName + SIGNATURE_FILE_EXT;

        // Verify the signature.
        auto valid = verifyFile(file, sigFile, publicKeyFile, hashType);
        if (valid == false)
        {
            log<level::ERR>("Image file Signature Validation failed",
                            entry("IMAGE=%s", pnorFileName.c_str()));
            return false;
        }

        log<level::DEBUG>("Successfully completed Signature validation.");

        return true;
    }
    catch (const InternalFailure& e)
    {
        return false;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(e.what());
        return false;
    }
}

bool Signature::systemLevelVerify()
{
    // Get available key types from the system.
    auto keyTypes = getAvailableKeyTypesFromSystem();
    if (keyTypes.empty())
    {
        log<level::ERR>("Missing Signature configuration data in system");
        elog<InternalFailure>();
    }

    // Build publickey and its signature file name.
    std::filesystem::path pkeyFile(imageDirPath / PUBLICKEY_FILE_NAME);
    std::filesystem::path pkeyFileSig(pkeyFile);
    pkeyFileSig.replace_extension(SIGNATURE_FILE_EXT);

    // Build manifest and its signature file name.
    std::filesystem::path manifestFile(imageDirPath / MANIFEST_FILE);
    std::filesystem::path manifestFileSig(manifestFile);
    manifestFileSig.replace_extension(SIGNATURE_FILE_EXT);

    auto valid = false;

    // Verify the file signature with available key types
    // public keys and hash function.
    // For any internal failure during the key/hash pair specific
    // validation, should continue the validation with next
    // available Key/hash pair.
    for (const auto& keyType : keyTypes)
    {
        auto keyHashPair = getKeyHashFileNames(keyType);
        auto keyValues = Version::getValue(keyHashPair.first,
                                           {{hashFunctionTag, " "}});
        auto hashFunc = keyValues.at(hashFunctionTag);

        try
        {
            // Verify manifest file signature
            valid = verifyFile(manifestFile, manifestFileSig,
                               keyHashPair.second, hashFunc);
            if (valid)
            {
                // Verify publickey file signature.
                valid = verifyFile(pkeyFile, pkeyFileSig, keyHashPair.second,
                                   hashFunc);
                if (valid)
                {
                    break;
                }
            }
        }
        catch (const InternalFailure& e)
        {
            valid = false;
        }
    }
    return valid;
}

bool Signature::verifyFile(const std::filesystem::path& file,
                           const std::filesystem::path& sigFile,
                           const std::filesystem::path& publicKey,
                           const std::string& hashFunc)
{
    // Check existence of the files in the system.
    if (!(std::filesystem::exists(file) && std::filesystem::exists(sigFile)))
    {
        log<level::ERR>("Failed to find the Data or signature file.",
                        entry("FILE=%s", file.c_str()));
        elog<InternalFailure>();
    }

    // Create RSA.
    auto publicRSA = createPublicRSA(publicKey);
    if (!publicRSA)
    {
        log<level::ERR>("Failed to create RSA",
                        entry("FILE=%s", publicKey.c_str()));
        elog<InternalFailure>();
    }

    // Initializes a digest context.
    EVP_MD_CTX_Ptr rsaVerifyCtx(EVP_MD_CTX_new(), ::EVP_MD_CTX_free);

    // Adds all digest algorithms to the internal table
    OpenSSL_add_all_digests();

    // Create Hash structure.
    auto hashStruct = EVP_get_digestbyname(hashFunc.c_str());
    if (!hashStruct)
    {
        log<level::ERR>("EVP_get_digestbynam: Unknown message digest",
                        entry("HASH=%s", hashFunc.c_str()));
        elog<InternalFailure>();
    }

    auto result = EVP_DigestVerifyInit(rsaVerifyCtx.get(), nullptr, hashStruct,
                                       nullptr, publicRSA.get());

    if (result <= 0)
    {
        log<level::ERR>("Error occurred during EVP_DigestVerifyInit",
                        entry("ERRCODE=%lu", ERR_get_error()));
        elog<InternalFailure>();
    }

    // Hash the data file and update the verification context
    auto size = std::filesystem::file_size(file);
    auto dataPtr = mapFile(file, size);

    result = EVP_DigestVerifyUpdate(rsaVerifyCtx.get(), dataPtr(), size);
    if (result <= 0)
    {
        log<level::ERR>("Error occurred during EVP_DigestVerifyUpdate",
                        entry("ERRCODE=%lu", ERR_get_error()));
        elog<InternalFailure>();
    }

    // Verify the data with signature.
    size = std::filesystem::file_size(sigFile);
    auto signature = mapFile(sigFile, size);

    result = EVP_DigestVerifyFinal(
        rsaVerifyCtx.get(), reinterpret_cast<unsigned char*>(signature()),
        size);

    // Check the verification result.
    if (result < 0)
    {
        log<level::ERR>("Error occurred during EVP_DigestVerifyFinal",
                        entry("ERRCODE=%lu", ERR_get_error()));
        elog<InternalFailure>();
    }

    if (result == 0)
    {
        log<level::ERR>("EVP_DigestVerifyFinal:Signature validation failed",
                        entry("PATH=%s", sigFile.c_str()));
        return false;
    }
    return true;
}

inline EVP_PKEY_Ptr
    Signature::createPublicRSA(const std::filesystem::path& publicKey)
{
    auto size = std::filesystem::file_size(publicKey);

    // Read public key file
    auto data = mapFile(publicKey, size);

    BIO_MEM_Ptr keyBio(BIO_new_mem_buf(data(), -1), &::BIO_free);
    if (keyBio.get() == nullptr)
    {
        log<level::ERR>("Failed to create new BIO Memory buffer");
        elog<InternalFailure>();
    }

    return {PEM_read_bio_PUBKEY(keyBio.get(), nullptr, nullptr, nullptr),
            &::EVP_PKEY_free};
}

CustomMap Signature::mapFile(const std::filesystem::path& path, size_t size)
{
    CustomFd fd(open(path.c_str(), O_RDONLY));

    return CustomMap(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd(), 0),
                     size);
}

} // namespace image
} // namespace software
} // namespace openpower
