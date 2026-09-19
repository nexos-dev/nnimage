/*
    ImageCmd.cxx - contains ImageCmd frontend test cases
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

TEST_CASE ("ImageCmd creates an image from command-line properties")
{
    FrontendOptions opts;
    opts.imgSize = "64MiB";
    opts.bootMode = "bios";
    opts.imgType = "gpt";
    opts.props = {"boot_mode=efi"};
    opts.partSpecs.values = {"size=32MiB,fs_type=ext4,prefix=\"root\""};

    ImageCmd frontend (opts);
    REQUIRE (frontend.Parse());

    auto images = frontend.GetImages();
    REQUIRE (images.size() == 1);
    const Image& image = *images.front();
    CHECK (image.GetName().empty());
    CHECK (image.GetSpec().size == 64ULL * 1024 * 1024);
    CHECK (image.GetSpec().bootMode == BootMode::Efi);
    CHECK (image.GetPartitions().size() == 1);

    const PartSpec& part = image.GetPartitions().front()->GetSpec();
    CHECK (part.size == 32ULL * 1024 * 1024);
    CHECK (part.format == "ext4");
    CHECK (part.prefix == "root");
}

TEST_CASE ("ImageCmd rejects invalid size units")
{
    FrontendOptions opts;
    opts.imgSize = "64Frobs";

    ImageCmd frontend (opts);
    CHECK_FALSE (frontend.Parse());
}

TEST_CASE ("ImageCmd rejects malformed image properties")
{
    FrontendOptions opts;
    opts.props = {"fileext"};

    ImageCmd frontend (opts);
    CHECK_FALSE (frontend.Parse());
}

TEST_CASE ("ImageCmd rejects values containing multiple tokens")
{
    FrontendOptions opts;
    opts.bootMode = "bios efi";

    ImageCmd frontend (opts);
    CHECK_FALSE (frontend.Parse());
}
