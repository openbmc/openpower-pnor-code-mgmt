// SPDX-License-Identifier: Apache-2.0

#include "../functions.hpp"

#include <stdlib.h>

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace std::string_literals;

TEST(GetExtensionsForIbmCompatibleSystem, testSingleMatch)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-foo"s, {".EXT"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-foo"s}, extensions;
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".EXT"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testSingleNoMatchNoModify)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-bar"s, {".EXT"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-foo"s},
        extensions{"foo"s};
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_FALSE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{"foo"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testMatchModify)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-bar"s, {".BAR"s}},
        {"system-foo"s, {".FOO"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-foo"s},
        extensions{"foo"s};
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".FOO"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testEmpty)
{
    std::map<std::string, std::vector<std::string>> extensionMap;
    std::vector<std::string> compatibleSystem{"system-foo"s},
        extensions{"foo"s};
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_FALSE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{"foo"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testEmptyEmpty)
{
    std::map<std::string, std::vector<std::string>> extensionMap;
    std::vector<std::string> compatibleSystem, extensions{"foo"s};
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_FALSE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{"foo"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testMatchMultiCompat)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-bar"s, {".BAR"s}},
        {"system-foo"s, {".FOO"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-foo"s, "system"},
        extensions;
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".FOO"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testMultiMatchMultiCompat)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-bar"s, {".BAR"s}},
        {"system-foo"s, {".FOO"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-foo"s, "system-bar"},
        extensions;
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".FOO"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testMultiMatchMultiCompat2)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-foo"s, {".FOO"s}},
        {"system-bar"s, {".BAR"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-foo"s, "system-bar"},
        extensions;
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".FOO"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testMultiMatchMultiCompat3)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-bar"s, {".BAR"s}},
        {"system-foo"s, {".FOO"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-bar", "system-foo"s},
        extensions;
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".BAR"s});
}

TEST(GetExtensionsForIbmCompatibleSystem, testMultiMatchMultiCompat4)
{
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"system-foo"s, {".FOO"s}},
        {"system-bar"s, {".BAR"s}},
    }};
    std::vector<std::string> compatibleSystem{"system-bar", "system-foo"s},
        extensions;
    auto found =
        functions::process_hostfirmware::getExtensionsForIbmCompatibleSystem(
            extensionMap, compatibleSystem, extensions);
    EXPECT_TRUE(found);
    EXPECT_EQ(extensions, std::vector<std::string>{".BAR"s});
}

TEST(MaybeCall, noMatch)
{
    bool called = false;
    auto callback = [&called](const auto&) { called = true; };
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces{{
            {"foo"s, {{"bar"s, std::vector<std::string>{"foo"s}}}},
        }};
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::move(callback));
    EXPECT_FALSE(found);
    EXPECT_FALSE(called);
}

TEST(MaybeCall, match)
{
    bool called = false;
    std::vector<std::string> sys;
    auto callback = [&called, &sys](const auto& system) {
        sys = system;
        called = true;
    };
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces{{
            {"xyz.openbmc_project.Configuration.IBMCompatibleSystem"s,
             {{"Names"s, std::vector<std::string>{"foo"s}}}},
        }};
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::move(callback));
    EXPECT_TRUE(found);
    EXPECT_TRUE(called);
    EXPECT_EQ(sys, std::vector<std::string>{"foo"s});
}

TEST(MaybeCall, missingNames)
{
    bool called = false;
    auto callback = [&called](const auto&) { called = true; };
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces{{
            {"xyz.openbmc_project.Configuration.IBMCompatibleSystem"s, {}},
        }};
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::move(callback));
    EXPECT_TRUE(found);
    EXPECT_FALSE(called);
}

TEST(MaybeCall, emptyCallbackFound)
{
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces{{
            {"xyz.openbmc_project.Configuration.IBMCompatibleSystem"s,
             {{"Names"s, std::vector<std::string>{"foo"s}}}},
        }};
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::function<void(std::vector<std::string>)>());
    EXPECT_TRUE(found);
}

TEST(MaybeCall, emptyCallbackNotFound)
{
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces{{
            {"foo"s, {{"Names"s, std::vector<std::string>{"foo"s}}}},
        }};
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::function<void(std::vector<std::string>)>());
    EXPECT_FALSE(found);
}

TEST(MaybeCall, emptyInterfaces)
{
    bool called = false;
    auto callback = [&called](const auto&) { called = true; };
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces;
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::move(callback));
    EXPECT_FALSE(found);
    EXPECT_FALSE(called);
}

TEST(MaybeCall, emptyInterfacesEmptyCallback)
{
    std::map<std::string,
             std::map<std::string, std::variant<std::vector<std::string>>>>
        interfaces;
    auto found = functions::process_hostfirmware::maybeCall(
        interfaces, std::function<void(std::vector<std::string>)>());
    EXPECT_FALSE(found);
}

TEST(WriteLink, testLinkNoDelete)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);
    bool called = false;
    auto callback = [&called](const auto&, auto&) { called = true; };
    std::filesystem::path linkPath = workdir / "link";
    std::filesystem::path targetPath = workdir / "target";
    std::ofstream link{linkPath};
    functions::process_hostfirmware::writeLink(linkPath.filename(), targetPath,
                                               callback);
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(called);
}

TEST(WriteLink, testLinkDelete)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);
    bool called = false;
    auto callback = [&called](const auto&, auto&) { called = true; };
    auto linkPath = workdir / "link";
    auto targetPath = workdir / "target";
    std::ofstream link{linkPath}, target{targetPath};
    functions::process_hostfirmware::writeLink(linkPath.filename(), targetPath,
                                               callback);
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(called);
}

TEST(WriteLink, testLinkFailDeleteDir)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);
    std::error_code ec;
    std::filesystem::path callbackPath;
    auto callback = [&ec, &callbackPath](const auto& p, auto& _ec) {
        ec = _ec;
        callbackPath = p;
    };
    auto targetPath = workdir / "target";
    std::filesystem::create_directory(targetPath);
    auto linkPath = workdir / "link";
    auto filePath = targetPath / "file";
    std::ofstream link{linkPath}, file{filePath};
    functions::process_hostfirmware::writeLink(linkPath.filename(), targetPath,
                                               callback);
    std::filesystem::remove_all(workdir);
    EXPECT_EQ(ec.value(), ENOTEMPTY);
    EXPECT_EQ(callbackPath, targetPath);
}

TEST(WriteLink, testLinkPathNotExist)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);
    std::error_code ec;
    std::filesystem::path callbackPath;
    auto callback = [&ec, &callbackPath](const auto& p, auto& _ec) {
        ec = _ec;
        callbackPath = p;
    };
    auto linkPath = workdir / "baz";
    auto targetPath = workdir / "foo/bar/foo";
    functions::process_hostfirmware::writeLink(linkPath.filename(), targetPath,
                                               callback);
    std::filesystem::remove_all(workdir);
    EXPECT_EQ(ec.value(), ENOENT);
    EXPECT_EQ(callbackPath, targetPath);
}

TEST(FindLinks, testNoLinks)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);

    bool callbackCalled = false, errorCallbackCalled = false;
    auto callback = [&callbackCalled](const auto&, const auto&, const auto&) {
        callbackCalled = true;
    };
    auto errorCallback = [&errorCallbackCalled](const auto&, auto&) {
        errorCallbackCalled = true;
    };

    std::vector<std::string> extensions;
    functions::process_hostfirmware::findLinks(workdir, extensions,
                                               errorCallback, callback);
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(errorCallbackCalled);
    EXPECT_FALSE(callbackCalled);
}

TEST(FindLinks, testOneFound)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);
    std::filesystem::path callbackPath, callbackLink;

    bool errorCallbackCalled = false;
    auto callback = [&callbackPath, &callbackLink](
                        const auto& p1, const auto& p2, const auto&) {
        callbackPath = p1;
        callbackLink = p2;
    };
    auto errorCallback = [&errorCallbackCalled](const auto&, auto&) {
        errorCallbackCalled = true;
    };

    auto filePath = workdir / "foo.foo";
    std::ofstream file{filePath};
    std::vector<std::string> extensions{".foo"s};
    functions::process_hostfirmware::findLinks(workdir, extensions,
                                               errorCallback, callback);
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(errorCallbackCalled);
    EXPECT_EQ(callbackLink, workdir / "foo");
    EXPECT_EQ(callbackPath, filePath.filename());
}

TEST(FindLinks, testNoExtensions)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);
    std::filesystem::path callbackPath, callbackLink;

    bool errorCallbackCalled = false, callbackCalled = false;
    auto callback = [&callbackCalled](const auto&, const auto&, const auto&) {
        callbackCalled = true;
    };
    auto errorCallback = [&errorCallbackCalled](const auto&, auto&) {
        errorCallbackCalled = true;
    };

    auto filePath = workdir / "foo.foo";
    std::ofstream file{filePath};
    std::vector<std::string> extensions;
    functions::process_hostfirmware::findLinks(workdir, extensions,
                                               errorCallback, callback);
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(errorCallbackCalled);
    EXPECT_FALSE(callbackCalled);
}

TEST(FindLinks, testEnoent)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);

    std::error_code ec;
    bool called = false;
    std::filesystem::path callbackPath;
    auto callback = [&called](const auto&, const auto&, const auto&) {
        called = true;
    };
    auto errorCallback = [&ec, &callbackPath](const auto& p, auto& _ec) {
        ec = _ec;
        callbackPath = p;
    };

    std::vector<std::string> extensions;
    auto dir = workdir / "baz";
    functions::process_hostfirmware::findLinks(dir, extensions, errorCallback,
                                               callback);
    std::filesystem::remove_all(workdir);
    EXPECT_EQ(ec.value(), ENOENT);
    EXPECT_EQ(callbackPath, dir);
    EXPECT_FALSE(called);
}

TEST(FindLinks, testEmptyCallback)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);

    bool called = false;
    std::filesystem::path callbackPath;
    auto errorCallback = [&called](const auto&, auto&) { called = true; };

    auto filePath = workdir / "foo.foo";
    std::ofstream file{filePath};

    std::vector<std::string> extensions{".foo"s};
    functions::process_hostfirmware::findLinks(
        workdir, extensions, errorCallback,
        functions::process_hostfirmware::LinkCallbackType());
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(called);
    EXPECT_NO_THROW();
}

TEST(FindLinks, testEmptyErrorCallback)
{
    std::array<char, 15> tmpl{"/tmp/tmpXXXXXX"};
    std::filesystem::path workdir = mkdtemp(&tmpl[0]);

    bool called = false;
    auto callback = [&called](const auto&, const auto&, const auto&) {
        called = true;
    };

    std::vector<std::string> extensions;
    auto dir = workdir / "baz";
    functions::process_hostfirmware::findLinks(
        dir, extensions, functions::process_hostfirmware::ErrorCallbackType(),
        callback);
    std::filesystem::remove_all(workdir);
    EXPECT_FALSE(called);
    EXPECT_NO_THROW();
}
