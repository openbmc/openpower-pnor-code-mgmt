#include "msl_verify.hpp"

#include <gtest/gtest.h>

namespace openpower
{
namespace software
{
namespace image
{

class MinimumShipLevelTest : public testing::Test
{
  protected:
    std::string minShipLevel = "v.2.2";
    std::unique_ptr<MinimumShipLevel> minimumShipLevel;

    virtual void SetUp()
    {
        minimumShipLevel = std::make_unique<MinimumShipLevel>(minShipLevel);
    }
};

TEST_F(MinimumShipLevelTest, compare)
{
    MinimumShipLevel::Version min;
    MinimumShipLevel::Version actual;

    min = {3, 5, 7};

    // actual = min
    actual = {3, 5, 7};
    EXPECT_EQ(0, minimumShipLevel->compare(actual, min));

    // actual < min
    actual = {3, 5, 6};
    EXPECT_EQ(-1, minimumShipLevel->compare(actual, min));
    actual = {3, 4, 7};
    EXPECT_EQ(-1, minimumShipLevel->compare(actual, min));
    actual = {2, 5, 7};
    EXPECT_EQ(-1, minimumShipLevel->compare(actual, min));

    // actual > min
    actual = {3, 5, 8};
    EXPECT_EQ(1, minimumShipLevel->compare(actual, min));
    actual = {3, 6, 7};
    EXPECT_EQ(1, minimumShipLevel->compare(actual, min));
    actual = {4, 5, 7};
    EXPECT_EQ(1, minimumShipLevel->compare(actual, min));
}

TEST_F(MinimumShipLevelTest, parse)
{
    MinimumShipLevel::Version version;
    std::string versionStr;

    versionStr = "nomatch-1.2.3-abc";
    minimumShipLevel->parse(versionStr, version);
    EXPECT_EQ(0, version.major);
    EXPECT_EQ(0, version.minor);
    EXPECT_EQ(0, version.rev);

    versionStr = "xyzformat-v1.2.3-4.5abc";
    minimumShipLevel->parse(versionStr, version);
    EXPECT_EQ(1, version.major);
    EXPECT_EQ(2, version.minor);
    EXPECT_EQ(3, version.rev);

    versionStr = "xyformat-system-v6.7-abc";
    minimumShipLevel->parse(versionStr, version);
    EXPECT_EQ(6, version.major);
    EXPECT_EQ(7, version.minor);
    EXPECT_EQ(0, version.rev);

    versionStr = "Vendor-Model-v-4.1.01";
    minimumShipLevel->parse(versionStr, version);
    EXPECT_EQ(4, version.major);
    EXPECT_EQ(1, version.minor);
    EXPECT_EQ(1, version.rev);

    versionStr = "Vendor-Model-v-4.1-abc";
    minimumShipLevel->parse(versionStr, version);
    EXPECT_EQ(4, version.major);
    EXPECT_EQ(1, version.minor);
    EXPECT_EQ(0, version.rev);
}

} // namespace image
} // namespace software
} // namespace openpower
