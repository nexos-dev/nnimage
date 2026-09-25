/*
    ImageConf.cxx - contains image configuration parser test cases
    Copyright 2026 Jedidiah Thompson

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "doctest.h"
#include "include/Frontend.h"
#include "include/ImageParser.h"

#include <filesystem>
#include <fstream>
#include <string>

static bool ParseBlockFails (std::string data)
{
    ImageParser parser ("test.conf", std::move (data));
    return !parser.ParseBlock();
}

static std::filesystem::path MakeConfigFile (std::string_view tag, std::string_view content)
{
    auto dir =
        std::filesystem::temp_directory_path() / std::filesystem::path ("nnimage_imageconf_test_" + std::string (tag));
    std::filesystem::remove_all (dir);
    std::filesystem::create_directories (dir);

    auto path = dir / "config.conf";
    std::ofstream out (path);
    REQUIRE (out.is_open());
    out << content;
    out.close();
    return path;
}

TEST_CASE ("ImageParser parses identifier lists")
{
    ImageParser parser ("test.conf", "image test { partitions: test1, test2, test3; }");

    auto res = parser.ParseBlock();
    REQUIRE (res);
    REQUIRE (res.Value().has_value());

    auto prop = res.Value()->props.find ("partitions");
    REQUIRE (prop != res.Value()->props.end());
    auto list = prop->second.val.Get<ImageList>();
    REQUIRE (list.has_value());
    REQUIRE (list->size() == 3);
    CHECK ((*list)[0].Str() == "test1");
    CHECK ((*list)[1].Str() == "test2");
    CHECK ((*list)[2].Str() == "test3");
}

TEST_CASE ("ImageParser parses each scalar property type")
{
    ImageParser parser ("test.conf",
        "image test { id: value; text: \"hello\"; count: 42; size: 64MiB; enabled: true; disabled: false; }");
    auto res = parser.ParseBlock();
    REQUIRE (res);
    REQUIRE (res.Value().has_value());

    const auto& props = res.Value()->props;
    REQUIRE (props.find ("id") != props.end());
    REQUIRE (props.find ("text") != props.end());
    REQUIRE (props.find ("count") != props.end());
    REQUIRE (props.find ("size") != props.end());
    REQUIRE (props.find ("enabled") != props.end());
    REQUIRE (props.find ("disabled") != props.end());

    auto id = props.at ("id").val.Get<ImageId>();
    REQUIRE (id.has_value());
    CHECK (id->Str() == "value");

    auto text = props.at ("text").val.Get<std::string>();
    REQUIRE (text.has_value());
    CHECK (*text == "hello");

    auto count = props.at ("count").val.Get<uint64_t>();
    REQUIRE (count.has_value());
    CHECK (*count == 42);

    auto size = props.at ("size").val.Get<ImageNumId>();
    REQUIRE (size.has_value());
    CHECK (size->Get() == 64ULL * 1024 * 1024);

    auto enabled = props.at ("enabled").val.Get<bool>();
    REQUIRE (enabled.has_value());
    CHECK (*enabled);

    auto disabled = props.at ("disabled").val.Get<bool>();
    REQUIRE (disabled.has_value());
    CHECK_FALSE (*disabled);
}

TEST_CASE ("ImageParser parses multiple blocks")
{
    ImageParser parser ("test.conf", "image first { value: one; } partition second { size: 2MiB; }");

    auto first = parser.ParseBlock();
    REQUIRE (first);
    REQUIRE (first.Value().has_value());
    CHECK (first.Value()->type == "image");
    CHECK (first.Value()->name == "first");
    CHECK (first.Value()->props.find ("value") != first.Value()->props.end());

    auto second = parser.ParseBlock();
    REQUIRE (second);
    REQUIRE (second.Value().has_value());
    CHECK (second.Value()->type == "partition");
    CHECK (second.Value()->name == "second");
    CHECK (second.Value()->props.find ("size") != second.Value()->props.end());

    auto eof = parser.ParseBlock();
    REQUIRE (eof);
    CHECK_FALSE (eof.Value().has_value());
}

TEST_CASE ("ImageParser overwrites duplicate properties")
{
    ImageParser parser ("test.conf", "image test { value: first; value: second; }");

    auto res = parser.ParseBlock();
    REQUIRE (res);
    REQUIRE (res.Value().has_value());

    auto value = res.Value()->props.at ("value").val.Get<ImageId>();
    REQUIRE (value.has_value());
    CHECK (value->Str() == "second");
}

TEST_CASE ("ImageParser rejects a block without a name")
{
    CHECK (ParseBlockFails ("image { value: test; }"));
}

TEST_CASE ("ImageParser rejects a block without an opening brace")
{
    CHECK (ParseBlockFails ("image test value: test; }"));
}

TEST_CASE ("ImageParser rejects a block without a closing brace")
{
    CHECK (ParseBlockFails ("image test { value: test;"));
}

TEST_CASE ("ImageParser rejects a property without a colon")
{
    CHECK (ParseBlockFails ("image test { value test; }"));
}

TEST_CASE ("ImageParser rejects a property without a value")
{
    CHECK (ParseBlockFails ("image test { value: ; }"));
}

TEST_CASE ("ImageParser rejects a property without a semicolon")
{
    CHECK (ParseBlockFails ("image test { value: test }"));
}

TEST_CASE ("ImageParser rejects adjacent property values")
{
    ImageParser parser ("test.conf", "image test { partitions: test1 test2; }");

    CHECK_FALSE (parser.ParseBlock());
}

TEST_CASE ("ImageParser rejects a trailing list comma")
{
    ImageParser parser ("test.conf", "image test { partitions: test1,; }");

    CHECK_FALSE (parser.ParseBlock());
}

TEST_CASE ("ImageParser rejects a repeated list comma")
{
    ImageParser parser ("test.conf", "image test { partitions: test1,,test2; }");

    CHECK_FALSE (parser.ParseBlock());
}

TEST_CASE ("ImageParser rejects a list made from non-identifiers")
{
    CHECK (ParseBlockFails ("image test { values: 1, 2; }"));
}

TEST_CASE ("ImageParser rejects a list with mixed value types")
{
    CHECK (ParseBlockFails ("image test { values: first, 2; }"));
}

TEST_CASE ("ImageConf parses a valid image config and resolves partition references")
{
    const auto path = MakeConfigFile ("valid",
        "image test {\n"
        "    size: 128MiB;\n"
        "    partitions: root, swap;\n"
        "}\n"
        "partition root {\n"
        "    size: 32MiB;\n"
        "    fs_type: ext4;\n"
        "    is_boot: true;\n"
        "}\n"
        "partition swap {\n"
        "    size: 16MiB;\n"
        "    fs_type: swap;\n"
        "}\n");

    FrontendOptions opts;
    opts.confFile = path.string();

    ImageConf frontend (opts);
    REQUIRE (frontend.Parse());

    auto images = frontend.GetImages();
    REQUIRE (images.size() == 1);
    const Image& image = *images.front();
    CHECK (image.GetName() == "test");
    CHECK (image.GetSpec().size == 128ULL * 1024 * 1024);
    REQUIRE (image.GetPartitions().size() == 2);

    CHECK (image.GetPartitions()[0]->GetName() == "root");
    CHECK (image.GetPartitions()[1]->GetName() == "swap");
    CHECK (image.GetPartitions()[0]->GetSpec().format == "ext4");
    CHECK (image.GetPartitions()[0]->GetSpec().size == 32ULL * 1024 * 1024);
    REQUIRE (image.GetPartitions()[0]->GetSpec().isBoot.has_value());
    CHECK (image.GetPartitions()[0]->GetSpec().isBoot.value());

    std::filesystem::remove_all (path.parent_path());
}

TEST_CASE ("ImageConf resolves a boot_image reference between images")
{
    const auto path = MakeConfigFile ("boot_image",
        "image boot {\n"
        "    size: 16MiB;\n"
        "}\n"
        "image test {\n"
        "    size: 128MiB;\n"
        "    type: iso9660;\n"
        "    boot_image: boot;\n"
        "}\n");

    FrontendOptions opts;
    opts.confFile = path.string();

    ImageConf frontend (opts);
    REQUIRE (frontend.Parse());

    auto images = frontend.GetImages();
    REQUIRE (images.size() == 2);

    Image* bootImage = nullptr;
    Image* testImage = nullptr;
    for (auto& image : images)
    {
        if (image->GetName() == "boot")
            bootImage = image.get();
        else if (image->GetName() == "test")
            testImage = image.get();
    }
    REQUIRE (bootImage != nullptr);
    REQUIRE (testImage != nullptr);

    auto partType = testImage->Get<PartType> (ImgProp::PartType);
    REQUIRE (partType);
    REQUIRE (partType.Value().has_value());
    CHECK (*partType.Value() == PartType::Iso9660);

    // Parse() must resolve boot_image references without any extra manual step
    auto resolvedBootImage = testImage->Get<Image*> (ImgProp::BootImage);
    REQUIRE (resolvedBootImage);
    REQUIRE (resolvedBootImage.Value().has_value());
    CHECK (*resolvedBootImage.Value() == bootImage);

    std::filesystem::remove_all (path.parent_path());
}

TEST_CASE ("ImageConf rejects an image that references an undefined partition")
{
    const auto path = MakeConfigFile ("missing_partition",
        "image test {\n"
        "    size: 64MiB;\n"
        "    partitions: missing;\n"
        "}\n");

    FrontendOptions opts;
    opts.confFile = path.string();

    ImageConf frontend (opts);
    CHECK_FALSE (frontend.Parse());

    std::filesystem::remove_all (path.parent_path());
}

TEST_CASE ("ImageConf rejects unsupported block types and duplicate image names")
{
    auto badType = MakeConfigFile ("bad_block_type",
        "volume broken {\n"
        "    size: 32MiB;\n"
        "}\n");

    FrontendOptions opts1;
    opts1.confFile = badType.string();
    ImageConf badTypeFrontend (opts1);
    CHECK_FALSE (badTypeFrontend.Parse());

    auto duplicateImage = MakeConfigFile ("duplicate_image",
        "image dup {\n"
        "    size: 16MiB;\n"
        "}\n"
        "image dup {\n"
        "    size: 32MiB;\n"
        "}\n");

    FrontendOptions opts2;
    opts2.confFile = duplicateImage.string();
    ImageConf duplicateFrontend (opts2);
    CHECK_FALSE (duplicateFrontend.Parse());

    std::filesystem::remove_all (badType.parent_path());
    std::filesystem::remove_all (duplicateImage.parent_path());
}

TEST_CASE ("ImageConf stress test with many partitions and repeated image parsing")
{
    constexpr size_t partitionCount = 128;

    std::string config;
    config += "image stress {\n";
    config += "    size: 512MiB;\n";
    config += "    partitions:";
    for (size_t i = 0; i < partitionCount; ++i)
    {
        if (i != 0)
            config += ",";
        config += " p" + std::to_string (i);
    }
    config += ";\n";
    config += "}\n";

    for (size_t i = 0; i < partitionCount; ++i)
    {
        config += "partition p" + std::to_string (i) + " {\n";
        config += "    size: 4MiB;\n";
        config += "    fs_type: ext4;\n";
        if (i % 4 == 0)
            config += "    is_boot: true;\n";
        config += "}\n";
    }

    const auto path = MakeConfigFile ("stress", config);

    FrontendOptions opts;
    opts.confFile = path.string();

    ImageConf frontend (opts);
    REQUIRE (frontend.Parse());

    auto images = frontend.GetImages();
    REQUIRE (images.size() == 1);
    REQUIRE (images.front()->GetPartitions().size() == partitionCount);

    std::filesystem::remove_all (path.parent_path());
}
