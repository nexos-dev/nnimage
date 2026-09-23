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
#include "include/ImageParser.h"

static bool ParseBlockFails (std::string data)
{
    ImageParser parser ("test.conf", std::move (data));
    return !parser.ParseBlock();
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
