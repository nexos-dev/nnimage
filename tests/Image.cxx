/*
    Image.cxx - contains image/partition/component test cases
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
#include "include/Image.h"

#include <string>
#include <type_traits>

// A minimal component used purely to exercise Image::AddComponent/CheckComponent/GetComponent.
// It owns no properties of its own, so getRegistry() can just return an empty table.
class TestComponent : public Component
{
  public:
    TestComponent (Image& img) : Component (CompType::Format, img)
    {}

    ImageResult Validate() override
    {
        return Success();
    }

  protected:
    const CompConfRegistry& getMainRegistry() const override
    {
        static const CompConfRegistry empty{};
        return empty;
    }

    const CompConfRegistry& getSubRegistry() const override
    {
        static const CompConfRegistry empty{};
        return empty;
    }
};

class OtherTestComponent : public Component
{
  public:
    OtherTestComponent (Image& img) : Component (CompType::Encryption, img)
    {}

    ImageResult Validate() override
    {
        return Success();
    }

  protected:
    const CompConfRegistry& getMainRegistry() const override
    {
        static const CompConfRegistry empty{};
        return empty;
    }

    const CompConfRegistry& getSubRegistry() const override
    {
        static const CompConfRegistry empty{};
        return empty;
    }
};

// Helper to build a parsed ImageNumId (mirrors what ImageCmd::getSize does before handing it to
// Image::Set)
static ImageNumId MakeNumId (size_t num, std::string_view mul)
{
    ImageNumId id (num, mul);
    REQUIRE (id.Parse().IsOk());
    return id;
}

/********************
 *
 * ImgSpec / PartSpec are non-copyable and non-movable
 *
 *********************/

TEST_CASE ("ImgSpec and PartSpec are neither copyable nor assignable")
{
    static_assert (!std::is_copy_constructible_v<ImgSpec>);
    static_assert (!std::is_copy_assignable_v<ImgSpec>);
    static_assert (!std::is_copy_constructible_v<PartSpec>);
    static_assert (!std::is_copy_assignable_v<PartSpec>);
    CHECK (true);
}

/********************
 *
 * ImageNumId test cases
 *
 *********************/

TEST_CASE ("ImageNumId parses every supported multiplier")
{
    CHECK (MakeNumId (1, "B").Get() == 1);
    CHECK (MakeNumId (1, "KiB").Get() == 1024);
    CHECK (MakeNumId (1, "KB").Get() == 1000);
    CHECK (MakeNumId (1, "MiB").Get() == 1024 * 1024);
    CHECK (MakeNumId (1, "MB").Get() == 1000 * 1000);
    CHECK (MakeNumId (1, "GiB").Get() == 1024LL * 1024 * 1024);
    CHECK (MakeNumId (1, "GB").Get() == 1000LL * 1000 * 1000);
    CHECK (MakeNumId (128, "MiB").Get() == 128LL * 1024 * 1024);
}

TEST_CASE ("ImageNumId::Parse fails on an unrecognized multiplier")
{
    ImageNumId id (10, "Frobs");
    auto res = id.Parse();
    CHECK_FALSE (res.IsOk());
}

TEST_CASE ("ImageNumId::Parse fails on multiplication overflow")
{
    ImageNumId id (static_cast<size_t> (-1), "TiB");
    auto res = id.Parse();
    CHECK_FALSE (res.IsOk());
}

TEST_CASE ("ImageNumId::Get throws when accessed before a successful Parse")
{
    ImageNumId id;
    CHECK_THROWS_AS (id.Get(), std::runtime_error);

    ImageNumId badId (1, "NotAUnit");
    auto res = badId.Parse();
    REQUIRE_FALSE (res.IsOk());
    CHECK_THROWS_AS (badId.Get(), std::runtime_error);
}

/********************
 *
 * ImageVal test cases
 *
 *********************/

TEST_CASE ("ImageVal IsEmpty reflects the monostate default")
{
    ImageVal empty;
    CHECK (empty.IsEmpty());
    CHECK_FALSE (empty.Get<int64_t>().has_value());

    ImageVal withVal (static_cast<int64_t> (5));
    CHECK_FALSE (withVal.IsEmpty());
    REQUIRE (withVal.Get<int64_t>().has_value());
    CHECK (*withVal.Get<int64_t>() == 5);
}

TEST_CASE ("ImageVal::Get returns nullopt when requesting the wrong alternative")
{
    ImageVal val (std::string ("hi"));
    CHECK_FALSE (val.Get<int64_t>().has_value());
    CHECK_FALSE (val.Get<bool>().has_value());
    REQUIRE (val.Get<std::string>().has_value());
    CHECK (*val.Get<std::string>() == "hi");
}

TEST_CASE ("ImageVal stores and reports its source line")
{
    ImageVal val (std::string ("x"), 42);
    CHECK (val.GetLine() == 42);

    ImageVal noLine (std::string ("y"));
    CHECK (noLine.GetLine() == -1);
}

/********************
 *
 * NameRegistry test cases
 *
 *********************/

TEST_CASE ("NameRegistry resolves known names and reports Max for unknown ones")
{
    CHECK (Image::ResolveProp ("size") == ImgProp::Size);
    CHECK (Image::ResolveProp ("boot_mode") == ImgProp::BootMode);
    CHECK (Image::ResolveProp ("not_a_real_prop") == ImgProp::Max);

    CHECK (Image::GetPropName (ImgProp::Size) == "size");
    CHECK (Image::GetPropName (ImgProp::BootMode) == "boot_mode");
}

TEST_CASE ("NameRegistry resolves partition property names")
{
    CHECK (Partition::ResolveName ("start") == PartProp::Start);
    CHECK (Partition::ResolveName ("is_boot") == PartProp::IsBoot);
    CHECK (Partition::ResolveName ("bogus") == PartProp::Max);
}

/********************
 *
 * Image property test cases
 *
 *********************/

TEST_CASE ("Image name accessors work as expected")
{
    Image img ("disk1");
    CHECK (img.GetName() == "disk1");
    img.SetName ("disk2");
    CHECK (img.GetName() == "disk2");
}

TEST_CASE ("Image component setter/getter works")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::BootLoad, ImageId ("grub")).IsOk());
    auto optRes = img.Get<BootLoadType> (ImgProp::BootLoad);
    REQUIRE (optRes.GetValue().has_value());
    CHECK (*optRes.GetValue() == BootLoadType::Grub);

    auto res = img.GetComponent<BootLoadComp> (CompType::Boot);
    REQUIRE (res.IsOk());
    REQUIRE (res.GetValue()->GetBootType() == BootLoadType::Grub);
}

TEST_CASE ("Image::Set/Get round-trips the size property through ImageNumId")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (128, "MiB")).IsOk());

    auto res = img.Get<int64_t> (ImgProp::Size);
    REQUIRE (res.IsOk());
    REQUIRE (res.GetValue().has_value());
    CHECK (*res.GetValue() == 128LL * 1024 * 1024);
}

TEST_CASE ("Image::Set/Get by name resolves the property before dispatching")
{
    Image img ("disk1");
    REQUIRE (img.Set ("size", MakeNumId (2, "GiB")).IsOk());

    auto res = img.Get<int64_t> ("size");
    REQUIRE (res.IsOk());
    CHECK (*res.GetValue() == 2LL * 1024 * 1024 * 1024);
}

TEST_CASE ("Image::Set rejects an unrecognized property name")
{
    Image img ("disk1");
    auto res = img.Set ("not_a_real_prop", std::string ("x"));
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::InvalidImgProp);
    CHECK (res.GetError().RootFrame().msg.find ("disk1") != std::string::npos);
}

TEST_CASE ("Image::Set reports a type mismatch when the value's variant alternative is wrong")
{
    Image img ("disk1");
    auto res = img.Set (ImgProp::Size, std::string ("not a numid"));
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::PropTypeMismatch);
}

TEST_CASE ("Image::Set boot_mode resolves each supported keyword and rejects unknown ones")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("bios")).IsOk());
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("efi")).IsOk());
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("uefi")).IsOk());
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("none")).IsOk());

    auto res = img.Set (ImgProp::BootMode, ImageId ("not_a_mode"));
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::InvalidId);
}

TEST_CASE ("Image::IsSet reflects whether a property currently has a value")
{
    Image img ("disk1");
    auto res = img.IsSet (ImgProp::Size);
    REQUIRE (res.IsOk());
    CHECK_FALSE (res.GetValue());

    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")).IsOk());
    res = img.IsSet (ImgProp::Size);
    REQUIRE (res.IsOk());
    CHECK (res.GetValue());
}

TEST_CASE ("Image::SetDefaults fails when a property without a default is missing")
{
    Image img ("disk1");
    auto res = img.SetDefaults();
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::ImgMissingProp);
}

TEST_CASE ("Image::SetDefaults fills in defaulted properties without touching ones already set")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")).IsOk());
    REQUIRE (img.SetDefaults().IsOk());

    auto bootMode = img.Get<BootMode> (ImgProp::BootMode);
    REQUIRE (bootMode.IsOk());
    REQUIRE (bootMode.GetValue().has_value());
    CHECK (*bootMode.GetValue() == BootMode::None);

    auto size = img.Get<int64_t> (ImgProp::Size);
    REQUIRE (size.IsOk());
    CHECK (*size.GetValue() == 1024 * 1024);
}

TEST_CASE ("Image::SetDefaults does not overwrite an explicitly-set boot_mode")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")).IsOk());
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("efi")).IsOk());
    REQUIRE (img.SetDefaults().IsOk());

    auto bootMode = img.Get<BootMode> (ImgProp::BootMode);
    REQUIRE (bootMode.GetValue().has_value());
    CHECK (*bootMode.GetValue() == BootMode::Efi);
}

/********************
 *
 * Partition test cases
 *
 *********************/

TEST_CASE ("Partition name and spec accessors")
{
    Partition part ("part0");
    CHECK (part.GetName() == "part0");
    CHECK (part.GetSpec().name == "part0");
}

TEST_CASE ("Partition::Set/Get round-trips start, size, format, prefix and is_boot")
{
    Partition part ("part0");
    REQUIRE (part.Set ("start", MakeNumId (1, "MiB")).IsOk());
    REQUIRE (part.Set ("size", MakeNumId (16, "MiB")).IsOk());
    REQUIRE (part.Set ("format", std::string ("ext4")).IsOk());
    REQUIRE (part.Set ("prefix", std::string ("boot-")).IsOk());
    REQUIRE (part.Set ("is_boot", true).IsOk());

    CHECK (*part.Get<int64_t> ("start").GetValue() == 1024 * 1024);
    CHECK (*part.Get<int64_t> ("size").GetValue() == 16LL * 1024 * 1024);
    CHECK (*part.Get<std::string> ("format").GetValue() == "ext4");
    CHECK (*part.Get<std::string> ("prefix").GetValue() == "boot-");
    CHECK (*part.Get<bool> ("is_boot").GetValue() == true);
}

TEST_CASE ("Partition::Set rejects unknown properties and reports the partition name")
{
    Partition part ("part0");
    auto res = part.Set ("bogus", std::string ("x"));
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::InvalidPartProp);
    CHECK (res.GetError().RootFrame().msg.find ("part0") != std::string::npos);
}

TEST_CASE ("Partition::IsSet is false until the property is written")
{
    Partition part ("part0");
    CHECK_FALSE (part.IsSet ("format").GetValue());
    REQUIRE (part.Set ("format", std::string ("fat32")).IsOk());
    CHECK (part.IsSet ("format").GetValue());
}

TEST_CASE ("Partition::SetDefaults fills defaulted properties once the required ones are set")
{
    Partition part ("part0");
    REQUIRE (part.Set ("start", MakeNumId (1, "MiB")).IsOk());
    REQUIRE (part.Set ("size", MakeNumId (1, "MiB")).IsOk());
    REQUIRE (part.SetDefaults().IsOk());

    // "format"'s default is an empty string, and the getter treats an empty string as "not set", so
    // it correctly still reports as unset even after SetDefaults runs
    CHECK_FALSE (part.IsSet ("format").GetValue());
    CHECK_FALSE (part.Get<std::string> ("format").GetValue().has_value());

    // "is_boot" defaults to false, which the getter can distinguish from "unset"
    REQUIRE (part.IsSet ("is_boot").GetValue());
    CHECK (*part.Get<bool> ("is_boot").GetValue() == false);
}

TEST_CASE ("Partition::SetDefaults fails while start/size remain unset since they have no default")
{
    Partition part ("part0");
    auto res = part.SetDefaults();
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::PartMissingProp);
}

TEST_CASE ("Partition::Get reports a type mismatch when the requested C++ type does not match storage")
{
    Partition part ("part0");
    REQUIRE (part.Set ("format", std::string ("ext4")).IsOk());
    auto res = part.Get<int64_t> ("format");
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::PropTypeMismatch);
}

/********************
 *
 * Component test cases
 *
 *********************/

TEST_CASE ("Image::AddComponent/CheckComponent/GetComponent manage component ownership")
{
    Image img ("disk1");
    CHECK_FALSE (img.CheckComponent (CompType::Format));

    auto comp = std::make_unique<TestComponent> (img);
    TestComponent* rawPtr = comp.get();
    REQUIRE (img.AddComponent (std::move (comp)).IsOk());
    CHECK (img.CheckComponent (CompType::Format));

    auto getRes = img.GetComponent<TestComponent> (CompType::Format);
    REQUIRE (getRes.IsOk());
    CHECK (getRes.GetValue() == rawPtr);
}

TEST_CASE ("Image::AddComponent refuses to overwrite an already-populated slot")
{
    Image img ("disk1");
    REQUIRE (img.AddComponent (std::make_unique<TestComponent> (img)).IsOk());

    auto res = img.AddComponent (std::make_unique<TestComponent> (img));
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::ComponentOverwrite);
}

TEST_CASE ("Image::GetComponent reports an error when the dynamic type doesn't match")
{
    Image img ("disk1");
    REQUIRE (img.AddComponent (std::make_unique<TestComponent> (img)).IsOk());

    // Ask for a totally unrelated component type stored at the same slot - dynamic_cast must fail
    auto res = img.GetComponent<OtherTestComponent> (CompType::Format);
    CHECK_FALSE (res.IsOk());
    CHECK (res.GetError().RootFrame().code == ErrorCode::BadArgument);
}

TEST_CASE ("Image replays deferred component properties during validation")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")).IsOk());

    REQUIRE (img.Set (ImgProp::BootEmu, ImageId ("noemu")).IsOk());
    CHECK_FALSE (img.CheckComponent (CompType::PartType));

    REQUIRE (img.Set (ImgProp::PartType, ImageId ("iso9660")).IsOk());
    REQUIRE (img.SetDefaults().IsOk());
    img.AddPartition (std::make_unique<Partition> ("part0"));
    REQUIRE (img.Finalize().IsOk());

    auto bootEmu = img.Get<IsoBootEmu> (ImgProp::BootEmu);
    REQUIRE (bootEmu.IsOk());
    REQUIRE (bootEmu.GetValue().has_value());
    CHECK (*bootEmu.GetValue() == IsoBootEmu::NoEmu);
}

TEST_CASE ("Image partitions can be added and enumerated")
{
    Image img ("disk1");
    CHECK (img.GetPartitions().empty());

    img.AddPartition (std::make_unique<Partition> ("p1"));
    img.AddPartition (std::make_unique<Partition> ("p2"));

    REQUIRE (img.GetPartitions().size() == 2);
    CHECK (img.GetPartitions()[0]->GetName() == "p1");
    CHECK (img.GetPartitions()[1]->GetName() == "p2");
}

/********************
 *
 * ImageError message formatting test cases
 *
 *********************/

TEST_CASE ("ImageError formats a named-image message with the image name quoted")
{
    ImageError err (ErrorCode::InvalidImgProp, {{"prop", std::string ("bogus")}, {"name", std::string ("disk1")}});
    CHECK (err.RootFrame().msg.find ("\"disk1\"") != std::string::npos);
    CHECK (err.RootFrame().msg.find ("bogus") != std::string::npos);
}

TEST_CASE ("ImageError formats an anonymous-image message without a stray name")
{
    ImageError err (ErrorCode::InvalidImgProp, {{"prop", std::string ("bogus")}, {"name", std::string ("")}});
    CHECK (err.RootFrame().msg.find ("\"\"") == std::string::npos);
}

TEST_CASE ("ImageError::AddKey enriches the message with context added after construction")
{
    // std::unordered_map::insert (used by AddKey) never overwrites an already-present key, so AddKey
    // is only useful for adding keys that weren't supplied at construction time. The generic
    // file/line prefix in makeMessage is a good example: it's independent of the error code's
    // required keys, so it can be attached later without needing to satisfy assertKeys() again.
    ImageError err (ErrorCode::PropTypeMismatch, {{"prop", std::string ("boot_mode")}});
    CHECK (err.RootFrame().msg.find ("boot_mode") != std::string::npos);
    CHECK (err.RootFrame().msg.find ("myfile.conf") == std::string::npos);

    err.AddKey ({{"file", std::string ("myfile.conf")}});
    CHECK (err.RootFrame().msg.find ("myfile.conf:") != std::string::npos);
    CHECK (err.RootFrame().msg.find ("boot_mode") != std::string::npos);
}

/********************
 *
 * Stress tests
 *
 *********************/

TEST_CASE ("Image stress test with many partitions and repeated property writes")
{
    Image img ("bigdisk");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "TiB")).IsOk());

    constexpr int partCount = 2000;
    for (int i = 0; i < partCount; i++)
    {
        auto part = std::make_unique<Partition> ("part" + std::to_string (i));
        REQUIRE (part->Set ("start", MakeNumId (static_cast<size_t> (i + 1), "MiB")).IsOk());
        REQUIRE (part->Set ("size", MakeNumId (1, "MiB")).IsOk());
        img.AddPartition (std::move (part));
    }

    REQUIRE (img.GetPartitions().size() == static_cast<size_t> (partCount));
    for (int i = 0; i < partCount; i++)
    {
        auto& part = img.GetPartitions()[i];
        CHECK (part->GetName() == "part" + std::to_string (i));
        CHECK (*part->Get<int64_t> ("start").GetValue() == static_cast<int64_t> (i + 1) * 1024 * 1024);
    }

    // Repeatedly overwrite the same property many times and make sure the final value sticks
    for (int i = 0; i < 1000; i++)
        REQUIRE (img.Set (ImgProp::BootMode, ImageId (i % 2 == 0 ? "bios" : "efi")).IsOk());
    auto finalMode = img.Get<BootMode> (ImgProp::BootMode);
    CHECK (*finalMode.GetValue() == BootMode::Efi);
}
