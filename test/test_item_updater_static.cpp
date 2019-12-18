#include <string>
#include <utility>

#include <gtest/gtest.h>

using PartClear = std::pair<std::string, bool>;
namespace utils
{
extern std::vector<PartClear> getPartsToClear(const std::string& info);
}

TEST(TestItemUpdaterStatic, getPartsToClearOK)
{
    constexpr auto info =
        "Flash info:\n"
        "-----------\n"
        "Name          = /dev/mtd6\n"
        "Total size    = 64MB     Flags E:ECC, P:PRESERVED, R:READONLY, "
        "B:BACKUP\n"
        "Erase granule = 64KB           F:REPROVISION, V:VOLATILE, C:CLEARECC\n"
        "\n"
        "TOC@0x00000000 Partitions:\n"
        "-----------\n"
        "ID=00        part 0x00000000..0x00002000 (actual=0x00002000) "
        "[----------]\n"
        "ID=01        HBEL 0x00008000..0x0002c000 (actual=0x00024000) "
        "[E-----F-C-]\n"
        "ID=02       GUARD 0x0002c000..0x00031000 (actual=0x00005000) "
        "[E--P--F-C-]\n"
        "ID=03       NVRAM 0x00031000..0x000c1000 (actual=0x00090000) "
        "[---P--F---]\n"
        "ID=04     SECBOOT 0x000c1000..0x000e5000 (actual=0x00024000) "
        "[E--P------]\n"
        "ID=05       DJVPD 0x000e5000..0x0012d000 (actual=0x00048000) "
        "[E--P--F-C-]\n"
        "ID=06        MVPD 0x0012d000..0x001bd000 (actual=0x00090000) "
        "[E--P--F-C-]\n"
        "ID=07        CVPD 0x001bd000..0x00205000 (actual=0x00048000) "
        "[E--P--F-C-]\n"
        "ID=08         HBB 0x00205000..0x00305000 (actual=0x00100000) "
        "[EL--R-----]\n"
        "ID=09         HBD 0x00305000..0x00425000 (actual=0x00120000) "
        "[EL--------]\n"
        "ID=10         HBI 0x00425000..0x013e5000 (actual=0x00fc0000) "
        "[EL--R-----]\n"
        "ID=11         SBE 0x013e5000..0x014a1000 (actual=0x000bc000) "
        "[ELI-R-----]\n"
        "ID=12       HCODE 0x014a1000..0x015c1000 (actual=0x00120000) "
        "[EL--R-----]\n"
        "ID=13        HBRT 0x015c1000..0x01bc1000 (actual=0x00600000) "
        "[EL--R-----]\n"
        "ID=14     PAYLOAD 0x01bc1000..0x01cc1000 (actual=0x00100000) "
        "[-L--R-----]\n"
        "ID=15  BOOTKERNEL 0x01cc1000..0x02bc1000 (actual=0x00f00000) "
        "[-L--R-----]\n"
        "ID=16         OCC 0x02bc1000..0x02ce1000 (actual=0x00120000) "
        "[EL--R-----]\n"
        "ID=17     FIRDATA 0x02ce1000..0x02ce4000 (actual=0x00003000) "
        "[E-----F-C-]\n"
        "ID=18        CAPP 0x02ce4000..0x02d08000 (actual=0x00024000) "
        "[EL--R-----]\n"
        "ID=19     BMC_INV 0x02d08000..0x02d11000 (actual=0x00009000) "
        "[------F---]\n"
        "ID=20        HBBL 0x02d11000..0x02d18000 (actual=0x00007000) "
        "[EL--R-----]\n"
        "ID=21    ATTR_TMP 0x02d18000..0x02d20000 (actual=0x00008000) "
        "[------F---]\n"
        "ID=22   ATTR_PERM 0x02d20000..0x02d28000 (actual=0x00008000) "
        "[E-----F-C-]\n"
        "ID=23     VERSION 0x02d28000..0x02d2a000 (actual=0x00002000) "
        "[-L--R-----]\n"
        "ID=24 IMA_CATALOG 0x02d2a000..0x02d6a000 (actual=0x00040000) "
        "[EL--R-----]\n"
        "ID=25     RINGOVD 0x02d6a000..0x02d8a000 (actual=0x00020000) "
        "[----------]\n"
        "ID=26     WOFDATA 0x02d8a000..0x0308a000 (actual=0x00300000) "
        "[EL--R-----]\n"
        "ID=27 HB_VOLATILE 0x0308a000..0x0308f000 (actual=0x00005000) "
        "[E-----F-CV]\n"
        "ID=28        MEMD 0x0308f000..0x0309d000 (actual=0x0000e000) "
        "[EL--R-----]\n"
        "ID=29        SBKT 0x0309d000..0x030a1000 (actual=0x00004000) "
        "[EL--R-----]\n"
        "ID=30        HDAT 0x030a1000..0x030a9000 (actual=0x00008000) "
        "[EL--R-----]\n"
        "ID=31      UVISOR 0x030a9000..0x031a9000 (actual=0x00100000) "
        "[-L--R-----]\n"
        "ID=32      OCMBFW 0x031a9000..0x031f4000 (actual=0x0004b000) "
        "[EL--R-----]\n"
        "ID=33    UVBWLIST 0x031f4000..0x03204000 (actual=0x00010000) "
        "[-L--R-----]\n"
        "ID=34 BACKUP_PART 0x03ff7000..0x03fff000 (actual=0x00000000) "
        "[-----B----]";
    auto parts = utils::getPartsToClear(info);
    EXPECT_EQ(11, parts.size());

    EXPECT_EQ("HBEL", parts[0].first);
    EXPECT_TRUE(parts[0].second);

    EXPECT_EQ("GUARD", parts[1].first);
    EXPECT_TRUE(parts[1].second);

    EXPECT_EQ("NVRAM", parts[2].first);
    EXPECT_FALSE(parts[2].second);

    EXPECT_EQ("HB_VOLATILE", parts[10].first);
    EXPECT_TRUE(parts[10].second);
}

TEST(TestItemUpdaterStatic, getPartsToClearNotOK)
{
    // Verify the it does not crash on malformed texts
    constexpr auto info =
        "0x0308a000..0x0308f000(actual=0x00005000)"
        "[E-----F-CV]\n" // missing ID and name with F
        "ID=27 HB_VOLATILE 0x0308a000..0x0308f000 (actual=0x00005000) "
        "E-----F-CV]\n" // missing [
        "ID=22   ATTR_PERM 0x02d20000..0x02d28000 (actual=0x00008000) "
        "[E-----F-C-]\n" // The only valid one
        "ID=28        MEMD 0x0308f000..0x0309d000 (actual=0x0000e000) "
        "[----]\n" // missing flags
        "SBKT 0x0309d000..0x030a1000 (actual=0x00004000) "
        "[EL--R-----]\n"; // missing ID

    auto parts = utils::getPartsToClear(info);
    EXPECT_EQ(1, parts.size());
    EXPECT_EQ("ATTR_PERM", parts[0].first);
    EXPECT_TRUE(parts[0].second);
}

TEST(TestItemUpdaterStatic, getPartsToClearP8BasedOK)
{
    // NOTE: P8 doesn't support CLEARECC flags.
    constexpr auto info = R"(
Flash info:
-----------
Name          = /dev/mtd6
Total size    = 64MB	 Flags E:ECC, P:PRESERVED, R:READONLY, B:BACKUP
Erase granule = 64KB           F:REPROVISION, V:VOLATILE, C:CLEARECC

TOC@0x00000000 Partitions:
-----------
ID=00            part 0x00000000..0x00001000 (actual=0x00001000) [----R-----]
ID=01            HBEL 0x00008000..0x0002c000 (actual=0x00024000) [E-----F---]
ID=02           GUARD 0x0002c000..0x00031000 (actual=0x00005000) [E--P--F---]
ID=03             HBD 0x00031000..0x0008b000 (actual=0x0005a000) [E---R-----]
ID=04          HBD_RW 0x0008b000..0x00091000 (actual=0x00006000) [E---------]
ID=05           DJVPD 0x00091000..0x000d9000 (actual=0x00048000) [E-----F---]
ID=06            MVPD 0x000d9000..0x00169000 (actual=0x00090000) [E-----F---]
ID=07            CVPD 0x00169000..0x001b1000 (actual=0x00048000) [E-----F---]
ID=08             HBI 0x001b1000..0x00751000 (actual=0x005a0000) [EL--R-----]
ID=09            SBEC 0x00751000..0x007e1000 (actual=0x00090000) [E-I-R-----]
ID=10             SBE 0x007e1000..0x00829000 (actual=0x00048000) [E-I-R-----]
ID=11            WINK 0x00829000..0x00949000 (actual=0x00120000) [EL--R-----]
ID=12            HBRT 0x00949000..0x00ca9000 (actual=0x00360000) [EL--R-----]
ID=13         PAYLOAD 0x00ca9000..0x00d29000 (actual=0x00080000) [----R-----]
ID=14      BOOTKERNEL 0x00d29000..0x01ca9000 (actual=0x00f80000) [----R-----]
ID=15        ATTR_TMP 0x01ca9000..0x01cb1000 (actual=0x00008000) [------F---]
ID=16       ATTR_PERM 0x01cb1000..0x01cb9000 (actual=0x00008000) [E-----F---]
ID=17             OCC 0x01cb9000..0x01dd9000 (actual=0x00120000) [E---R-----]
ID=18            TEST 0x01dd9000..0x01de2000 (actual=0x00009000) [E---------]
ID=19           NVRAM 0x01de2000..0x01e72000 (actual=0x00090000) [---P--F---]
ID=20         FIRDATA 0x01e72000..0x01e75000 (actual=0x00003000) [E-----F---]
ID=21         BMC_INV 0x01e75000..0x01e7e000 (actual=0x00009000) [------F---]
ID=22            CAPP 0x01e7e000..0x01ea2000 (actual=0x00024000) [E---R-----]
ID=23         SECBOOT 0x01ea2000..0x01ec6000 (actual=0x00024000) [E--P------]
ID=24     IMA_CATALOG 0x01ec6000..0x01ecf000 (actual=0x00009000) [E---R-F---]
ID=25             HBB 0x01f60000..0x01ff0000 (actual=0x00090000) [EL--R-----]
ID=26         VERSION 0x01ff7000..0x01ff8000 (actual=0x00001000) [----R-----]
ID=27     BACKUP_PART 0x01ff8000..0x02000000 (actual=0x00000000) [----RB----]
ID=28      OTHER_SIDE 0x02000000..0x02008000 (actual=0x00000000) [----RB----]
    )";

    auto parts = utils::getPartsToClear(info);
    EXPECT_EQ(11, parts.size());

    EXPECT_EQ("HBEL", parts[0].first);
    EXPECT_TRUE(parts[0].second);

    EXPECT_EQ("GUARD", parts[1].first);
    EXPECT_TRUE(parts[1].second);

    EXPECT_EQ("NVRAM", parts[7].first);
    EXPECT_FALSE(parts[7].second);

    EXPECT_EQ("IMA_CATALOG", parts[10].first);
    EXPECT_TRUE(parts[10].second);
}
