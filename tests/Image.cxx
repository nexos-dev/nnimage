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

    ResNone Validate() override
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
    OtherTestComponent (Image& img) : Component (CompType::PartType, img)
    {}

    ResNone Validate() override
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
static ImageNumId MakeNumId (size_t num, std::string mul)
{
    ImageNumId id (num, std::move (mul));
    REQUIRE (id.Parse());
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
    CHECK (MakeNumId (1, "TiB").Get() == 1024LL * 1024 * 1024 * 1024);
    CHECK (MakeNumId (1, "TB").Get() == 1000LL * 1000 * 1000 * 1000);
    CHECK (MakeNumId (128, "MiB").Get() == 128LL * 1024 * 1024);
}

TEST_CASE ("ImageNumId::Parse fails on an unrecognized multiplier")
{
    ImageNumId id (10, "Frobs");
    auto res = id.Parse();
    CHECK_FALSE (res);
}

TEST_CASE ("ImageNumId::Parse fails on multiplication overflow")
{
    ImageNumId id (static_cast<size_t> (-1), "TiB");
    auto res = id.Parse();
    CHECK_FALSE (res);
}

TEST_CASE ("ImageNumId::Get throws when accessed before a successful Parse")
{
    ImageNumId id;
    CHECK_THROWS_AS (id.Get(), std::runtime_error);

    ImageNumId badId (1, "NotAUnit");
    auto res = badId.Parse();
    REQUIRE_FALSE (res);
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
    CHECK_FALSE (empty.Get<uint64_t>().has_value());

    ImageVal withVal (static_cast<uint64_t> (5));
    CHECK_FALSE (withVal.IsEmpty());
    REQUIRE (withVal.Get<uint64_t>().has_value());
    CHECK (*withVal.Get<uint64_t>() == 5);
}

TEST_CASE ("ImageVal::Get returns nullopt when requesting the wrong alternative")
{
    ImageVal val (std::string ("hi"));
    CHECK_FALSE (val.Get<uint64_t>().has_value());
    CHECK_FALSE (val.Get<bool>().has_value());
    REQUIRE (val.Get<std::string>().has_value());
    CHECK (*val.Get<std::string>() == "hi");
}

TEST_CASE ("ImageVal stores and reports its source location")
{
    ImageVal val (std::string ("x"), SourceLoc ("test2", 24));
    CHECK (val.GetLoc().line == 24);
    CHECK (val.GetLoc().file == "test2");

    ImageVal noLine (std::string ("y"));
    CHECK (noLine.GetLoc().line == -1);
    CHECK (noLine.GetLoc().file.empty());
}

TEST_CASE ("ImageVal::FromToken converts every token value and preserves its location")
{
    const SourceLoc loc ("tokens.conf", 7);

    auto identifier = ImageVal::FromToken ({TokenType::Identifier, std::string ("value"), loc});
    REQUIRE (identifier);
    REQUIRE (identifier.Value().Get<ImageId>().has_value());
    CHECK (identifier.Value().Get<ImageId>()->Str() == "value");
    CHECK (identifier.Value().GetLoc().line == 7);

    auto string = ImageVal::FromToken ({TokenType::String, std::string ("quoted"), loc});
    REQUIRE (string);
    CHECK (*string.Value().Get<std::string>() == "quoted");
    CHECK (string.Value().GetLoc().file == "tokens.conf");

    auto number = ImageVal::FromToken ({TokenType::Number, uint64_t{42}, loc});
    REQUIRE (number);
    CHECK (*number.Value().Get<uint64_t>() == 42);

    auto boolean = ImageVal::FromToken ({TokenType::True, true, loc});
    REQUIRE (boolean);
    CHECK (*boolean.Value().Get<bool>() == true);

    auto numId = ImageVal::FromToken ({TokenType::NumId, LexNumId{2, "MiB"}, loc});
    REQUIRE (numId);
    REQUIRE (numId.Value().Get<ImageNumId>().has_value());
    CHECK (numId.Value().Get<ImageNumId>()->Get() == 2LL * 1024 * 1024);
    CHECK (numId.Value().GetLoc().line == 7);

    auto invalidNumId = ImageVal::FromToken ({TokenType::NumId, LexNumId{2, "bad"}, loc});
    CHECK_FALSE (invalidNumId);
}

TEST_CASE ("ImageVal::Cast supports ID conversions and rejects unsupported conversions")
{
    ImageVal id (ImageId ("value"), SourceLoc ("cast.conf", 3));

    auto string = id.Cast (ImageVal::GetTypeIndex<std::string>());
    REQUIRE (string.Get<std::string>().has_value());
    CHECK (*string.Get<std::string>() == "value");
    CHECK (string.GetLoc().line == 3);

    auto list = id.Cast (ImageVal::GetTypeIndex<ImageList>());
    REQUIRE (list.Get<ImageList>().has_value());
    REQUIRE (list.Get<ImageList>()->size() == 1);
    CHECK ((*list.Get<ImageList>())[0].Str() == "value");
    CHECK (list.GetLoc().file == "cast.conf");

    CHECK (id.Cast (ImageVal::GetTypeIndex<bool>()).IsInvalid());
    CHECK (ImageVal (std::string ("quoted")).Cast (ImageVal::GetTypeIndex<ImageList>()).IsInvalid());
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

TEST_CASE ("Image and partition property iterators advance and remain at Max")
{
    ImgProp imgProp = ImgProp::Size;
    CHECK (imgProp++ == ImgProp::Size);
    CHECK (imgProp == ImgProp::BootMode);
    CHECK (++imgProp == ImgProp::PartType);
    imgProp = ImgProp::Max;
    CHECK (++imgProp == ImgProp::Max);
    CHECK (imgProp++ == ImgProp::Max);

    PartProp partProp = PartProp::Start;
    CHECK (partProp++ == PartProp::Start);
    CHECK (partProp == PartProp::Size);
    CHECK (++partProp == PartProp::Format);
    partProp = PartProp::Max;
    CHECK (++partProp == PartProp::Max);
    CHECK (partProp++ == PartProp::Max);
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
}

TEST_CASE ("Image file path and backend accessors preserve assigned values")
{
    Image img ("disk1");
    CHECK (img.GetFilePath().empty());

    img.SetFilePath ("images/disk1.img");
    CHECK (img.GetFilePath() == std::filesystem::path ("images/disk1.img"));

    CHECK (img.GetBackendType() == BackendType::None);
    CHECK (img.SetBackend (BackendType::Krun));
    CHECK (img.GetBackendType() == BackendType::Krun);
    CHECK_FALSE (img.SetBackend (BackendType::Xorriso));
    CHECK (img.GetBackendType() == BackendType::Krun);
}

TEST_CASE ("Image component setter/getter works")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::BootLoad, ImageId ("grub")));
    auto optRes = img.Get<BootLoadType> (ImgProp::BootLoad);
    REQUIRE (optRes.has_value());
    CHECK (*optRes == BootLoadType::Grub);

    auto* component = img.GetComponent<BootLoadComp> (CompType::Boot);
    REQUIRE (component != nullptr);
    REQUIRE (component->GetBootType() == BootLoadType::Grub);
}

TEST_CASE ("Image setter casts an ImageId to a string")
{
    Partition part ("part1");
    REQUIRE (part.Set (PartProp::Prefix, ImageId ("test")));

    auto res = part.Get<std::string> (PartProp::Prefix);
    REQUIRE (res.has_value());
    CHECK (*res == "test");
}

TEST_CASE ("Image::Set/Get round-trips the size property through ImageNumId")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (128, "MiB")));

    auto res = img.Get<uint64_t> (ImgProp::Size);
    REQUIRE (res.has_value());
    CHECK (*res == 128LL * 1024 * 1024);
}

TEST_CASE ("Image::Set/Get by name resolves the property before dispatching")
{
    Image img ("disk1");
    REQUIRE (img.Set ("size", MakeNumId (2, "GiB")));

    auto res = img.Get<uint64_t> ("size");
    REQUIRE (res);
    CHECK (*res.Value() == 2LL * 1024 * 1024 * 1024);
}

TEST_CASE ("Image::Get by name returns an error for an unrecognized property")
{
    Image img ("disk1");
    auto res = img.Get<uint64_t> ("not_a_real_prop");
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::InvalidImgProp);
}

TEST_CASE ("Image::Get throws when the requested type does not match the property")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")));
    CHECK_THROWS_AS (img.Get<std::string> (ImgProp::Size), ErrorException);
    CHECK_THROWS_AS (img.Get<std::string> ("size"), ErrorException);
}

TEST_CASE ("Image::Set rejects an unrecognized property name")
{
    Image img ("disk1");
    auto res = img.Set ("not_a_real_prop", std::string ("x"));
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::InvalidImgProp);
    CHECK (res.Error().RootFrame().msg.find ("disk1") != std::string::npos);
}

TEST_CASE ("Image::Set reports a type mismatch when the value's variant alternative is wrong")
{
    Image img ("disk1");
    auto res = img.Set (ImgProp::Size, std::string ("not a numid"));
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::PropTypeMismatch);
}

TEST_CASE ("Image::Set boot_mode resolves each supported keyword and rejects unknown ones")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("bios")));
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("efi")));
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("uefi")));
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("none")));

    auto res = img.Set (ImgProp::BootMode, ImageId ("not_a_mode"));
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::InvalidId);

    Image isoImage ("iso-image");
    REQUIRE (isoImage.Set (ImgProp::PartType, ImageId ("iso9660")));
    auto invalidEmulation = isoImage.Set (ImgProp::BootEmu, ImageId ("invalid"));
    CHECK_FALSE (invalidEmulation);
    CHECK (invalidEmulation.Error().RootFrame().code == ErrorCode::InvalidId);
}

TEST_CASE ("Image::Set rejects unknown component IDs and repeated component assignment")
{
    Image partTypeImage ("part-type-image");
    auto partType = partTypeImage.Set (ImgProp::PartType, ImageId ("unknown"));
    CHECK_FALSE (partType);
    CHECK (partType.Error().RootFrame().code == ErrorCode::InvalidId);

    Image formatImage ("format-image");
    auto format = formatImage.Set (ImgProp::Format, ImageId ("unknown"));
    CHECK_FALSE (format);
    CHECK (format.Error().RootFrame().code == ErrorCode::InvalidId);

    Image bootImage ("boot-image");
    auto boot = bootImage.Set (ImgProp::BootLoad, ImageId ("unknown"));
    CHECK_FALSE (boot);
    CHECK (boot.Error().RootFrame().code == ErrorCode::InvalidId);

    REQUIRE (bootImage.Set (ImgProp::BootLoad, ImageId ("grub")));
    auto duplicate = bootImage.Set (ImgProp::BootLoad, ImageId ("none"));
    CHECK_FALSE (duplicate);
    CHECK (duplicate.Error().RootFrame().code == ErrorCode::ComponentOverwrite);
}

TEST_CASE ("Image::IsSet by name reports an unrecognized property")
{
    Image img ("disk1");
    auto res = img.IsSet ("not_a_real_prop");
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::InvalidImgProp);
}

TEST_CASE ("Image::IsSet reflects whether a property currently has a value")
{
    Image img ("disk1");
    CHECK_FALSE (img.IsSet (ImgProp::Size));

    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")));
    CHECK (img.IsSet (ImgProp::Size));
}

TEST_CASE ("Image::SetDefaults fills in defaulted properties without touching ones already set")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")));
    img.SetDefaults();

    auto bootMode = img.Get<BootMode> (ImgProp::BootMode);
    REQUIRE (bootMode.has_value());
    CHECK (*bootMode == BootMode::None);

    auto size = img.Get<uint64_t> (ImgProp::Size);
    REQUIRE (size.has_value());
    CHECK (*size == 1024 * 1024);
}

TEST_CASE ("Image::SetDefaults does not overwrite an explicitly-set boot_mode")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")));
    REQUIRE (img.Set (ImgProp::BootMode, ImageId ("efi")));
    img.SetDefaults();

    auto bootMode = img.Get<BootMode> (ImgProp::BootMode);
    REQUIRE (bootMode.has_value());
    CHECK (*bootMode == BootMode::Efi);
}

TEST_CASE ("Image::SetDefaults initializes the format and its default partition type")
{
    Image rawImage ("raw-image");
    rawImage.SetDefaults();
    auto format = rawImage.Get<FormatType> (ImgProp::Format);
    REQUIRE (format.has_value());
    CHECK (*format == FormatType::Raw);
    auto partType = rawImage.Get<PartType> (ImgProp::PartType);
    REQUIRE (partType.has_value());
    CHECK (*partType == PartType::Gpt);

    Image isoImage ("iso-image");
    REQUIRE (isoImage.Set (ImgProp::Format, ImageId ("iso9660")));
    isoImage.SetDefaults();
    auto isoPartType = isoImage.Get<PartType> (ImgProp::PartType);
    REQUIRE (isoPartType.has_value());
    CHECK (*isoPartType == PartType::Iso9660);

    Image explicitPartTypeImage ("explicit-part-type-image");
    REQUIRE (explicitPartTypeImage.Set (ImgProp::PartType, ImageId ("mbr")));
    explicitPartTypeImage.SetDefaults();
    auto explicitPartType = explicitPartTypeImage.Get<PartType> (ImgProp::PartType);
    REQUIRE (explicitPartType.has_value());
    CHECK (*explicitPartType == PartType::Mbr);
}

/********************
 *
 * Image::AddImageRef / Image::GetRefs test cases
 *
 *********************/

TEST_CASE ("Image::AddImageRef records a name-based reference visible through GetRefs")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (img.Set (ImgProp::BootImage, ImageId ("bootdisk")));

    const auto& refs = img.GetRefs();
    REQUIRE (refs.size() == 1);
    CHECK (refs[0].ref.GetName() == "bootdisk");
}

TEST_CASE ("Image::GetRefs setter callback assigns the referenced image")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (img.Set (ImgProp::BootImage, ImageId ("bootdisk")));

    // Not yet resolved, so boot_image has no value
    auto before = img.Get<Image*> (ImgProp::BootImage);
    CHECK_FALSE (before.has_value());

    Image bootImg ("bootdisk");
    const auto& refs = img.GetRefs();
    REQUIRE (refs.size() == 1);
    refs[0].setter (&bootImg);

    auto after = img.Get<Image*> (ImgProp::BootImage);
    REQUIRE (after.has_value());
    CHECK (*after == &bootImg);
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
    REQUIRE (part.Set ("start", MakeNumId (1, "MiB")));
    REQUIRE (part.Set ("size", MakeNumId (16, "MiB")));
    REQUIRE (part.Set ("fs_type", ImageId ("ext4")));
    REQUIRE (part.Set ("prefix", std::string ("boot-")));
    REQUIRE (part.Set ("is_boot", true));

    CHECK (*part.Get<uint64_t> ("start").Value() == 1024 * 1024);
    CHECK (*part.Get<uint64_t> ("size").Value() == 16LL * 1024 * 1024);

    CHECK (*part.Get<std::string> ("fs_type").Value() == "ext4");
    CHECK (*part.Get<std::string> ("prefix").Value() == "boot-");
    CHECK (*part.Get<bool> ("is_boot").Value() == true);
}

TEST_CASE ("Partition::Set rejects unknown properties and reports the partition name")
{
    Partition part ("part0");
    auto res = part.Set ("bogus", std::string ("x"));
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::InvalidPartProp);
    CHECK (res.Error().RootFrame().msg.find ("part0") != std::string::npos);
}

TEST_CASE ("Partition::IsSet is false until the property is written")
{
    Partition part ("part0");
    CHECK_FALSE (part.IsSet ("fs_type").Value());
    REQUIRE (part.Set ("fs_type", ImageId ("fat32")));
    CHECK (part.IsSet ("fs_type").Value());
}

TEST_CASE ("Partition::SetDefaults fills defaulted properties once the required ones are set")
{
    Partition part ("part0");
    REQUIRE (part.Set ("start", MakeNumId (1, "MiB")));
    REQUIRE (part.Set ("size", MakeNumId (1, "MiB")));
    part.SetDefaults();

    // "format"'s default is an empty string, and the getter treats an empty string as "not set", so
    // it correctly still reports as unset even after SetDefaults runs
    CHECK_FALSE (part.IsSet ("fs_type").Value());
    CHECK_FALSE (part.Get<std::string> ("fs_type").Value().has_value());

    // "is_boot" defaults to false, which the getter can distinguish from "unset"
    REQUIRE (part.IsSet ("is_boot").Value());
    CHECK (*part.Get<bool> ("is_boot").Value() == false);
}

TEST_CASE ("Partition::Get reports a type mismatch when the requested C++ type does not match storage")
{
    Partition part ("part0");
    REQUIRE (part.Set ("fs_type", ImageId ("ext4")));
    CHECK_THROWS_AS (part.Get<ImageId> (PartProp::Format), ErrorException);
    CHECK_THROWS_AS (part.Get<ImageId> ("fs_type"), ErrorException);
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
    CHECK (img.GetComponent<TestComponent> (CompType::Format) == nullptr);

    auto comp = std::make_unique<TestComponent> (img);
    TestComponent* rawPtr = comp.get();
    REQUIRE (img.AddComponent (std::move (comp)));
    CHECK (img.CheckComponent (CompType::Format));

    auto* component = img.GetComponent<TestComponent> (CompType::Format);
    CHECK (component == rawPtr);
    CHECK_THROWS_AS (img.GetComponent<BootLoadComp> (CompType::Format), ErrorException);
}

TEST_CASE ("Image::GetComponent preserves constness for const images")
{
    Image img ("disk1");
    auto component = std::make_unique<TestComponent> (img);
    TestComponent* rawPtr = component.get();
    REQUIRE (img.AddComponent (std::move (component)));

    const Image& constImg = img;
    auto* retrieved = constImg.GetComponent<TestComponent> (CompType::Format);
    static_assert (std::is_same_v<decltype (retrieved), const TestComponent*>);
    CHECK (retrieved == rawPtr);
}

TEST_CASE ("Image::AddComponent refuses to overwrite an already-populated slot")
{
    Image img ("disk1");
    REQUIRE (img.AddComponent (std::make_unique<TestComponent> (img)));

    auto res = img.AddComponent (std::make_unique<TestComponent> (img));
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::ComponentOverwrite);
}

TEST_CASE ("Image replays deferred component properties during validation")
{
    Image img ("disk1");
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "MiB")));

    REQUIRE (img.Set (ImgProp::BootEmu, ImageId ("noemu")));
    CHECK_FALSE (img.CheckComponent (CompType::PartType));
    REQUIRE (img.Set (ImgProp::PartType, ImageId ("iso9660")));
    img.SetDefaults();
    img.AddPartition (std::make_unique<Partition> ("part0"));
    REQUIRE (img.Finalize());

    auto bootEmu = img.Get<IsoBootEmu> (ImgProp::BootEmu);
    REQUIRE (bootEmu.has_value());
    CHECK (*bootEmu == IsoBootEmu::NoEmu);
}

TEST_CASE ("Image::Finalize reports and retains unresolved deferred properties")
{
    Image img ("unresolved-image");
    REQUIRE (img.Set (ImgProp::BootEmu, ImageId ("noemu")));
    CHECK (img.CheckDeferred (CompType::PartType));

    auto res = img.Finalize();
    CHECK_FALSE (res);
    CHECK (res.Error().RootFrame().code == ErrorCode::UnresolvedDeferredProp);
    CHECK (img.CheckDeferred (CompType::PartType));
}

TEST_CASE ("Image retries failed deferred component properties when resolving all")
{
    Image img ("deferred-image");
    REQUIRE (img.Set (ImgProp::BootEmu, ImageId ("invalid")));
    CHECK (img.CheckDeferred (CompType::Max));
    REQUIRE (img.Set (ImgProp::PartType, ImageId ("iso9660")));
    CHECK (img.CheckDeferred (CompType::PartType));

    img.ResolveDeferred (CompType::Max);
    CHECK (img.CheckDeferred (CompType::Max));
    CHECK_FALSE (img.Finalize());
}

TEST_CASE ("ISO image finalization enforces boot emulation image requirements")
{
    Image missingBootImage ("missing-boot-image");
    REQUIRE (missingBootImage.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (missingBootImage.Set (ImgProp::BootEmu, ImageId ("hdd")));
    auto missingResult = missingBootImage.Finalize();
    CHECK_FALSE (missingResult);
    CHECK (missingResult.Error().RootFrame().msg.find ("missing boot image") != std::string::npos);

    Image bootImage ("boot-image");
    REQUIRE (bootImage.Set (ImgProp::PartType, ImageId ("gpt")));

    Image unexpectedBootImage ("unexpected-boot-image");
    REQUIRE (unexpectedBootImage.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (unexpectedBootImage.Set (ImgProp::BootEmu, ImageId ("noemu")));
    REQUIRE (unexpectedBootImage.Set (ImgProp::BootImage, ImageId ("boot-image")));
    const auto& unexpectedRefs = unexpectedBootImage.GetRefs();
    REQUIRE (unexpectedRefs.size() == 1);
    unexpectedRefs[0].setter (&bootImage);
    auto unexpectedResult = unexpectedBootImage.Finalize();
    CHECK_FALSE (unexpectedResult);
    CHECK (unexpectedResult.Error().RootFrame().msg.find ("not valid for emulation") != std::string::npos);

    Image validBootImage ("valid-boot-image");
    REQUIRE (validBootImage.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (validBootImage.Set (ImgProp::BootEmu, ImageId ("hdd")));
    REQUIRE (validBootImage.Set (ImgProp::BootImage, ImageId ("boot-image")));
    const auto& validRefs = validBootImage.GetRefs();
    REQUIRE (validRefs.size() == 1);
    validRefs[0].setter (&bootImage);
    CHECK (validBootImage.Finalize());

    Image incompatibleBootImage ("incompatible-boot-image");
    REQUIRE (incompatibleBootImage.Set (ImgProp::PartType, ImageId ("floppy")));
    Image incompatibleIso ("incompatible-iso");
    REQUIRE (incompatibleIso.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (incompatibleIso.Set (ImgProp::BootEmu, ImageId ("hdd")));
    REQUIRE (incompatibleIso.Set (ImgProp::BootImage, ImageId ("incompatible-boot-image")));
    const auto& incompatibleRefs = incompatibleIso.GetRefs();
    REQUIRE (incompatibleRefs.size() == 1);
    incompatibleRefs[0].setter (&incompatibleBootImage);
    auto incompatibleResult = incompatibleIso.Finalize();
    CHECK_FALSE (incompatibleResult);
    CHECK (incompatibleResult.Error().RootFrame().msg.find ("invalid for ISO9660") != std::string::npos);

    Image untypedBootImage ("untyped-boot-image");
    Image untypedIso ("untyped-iso");
    REQUIRE (untypedIso.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (untypedIso.Set (ImgProp::BootEmu, ImageId ("hdd")));
    REQUIRE (untypedIso.Set (ImgProp::BootImage, ImageId ("untyped-boot-image")));
    const auto& untypedRefs = untypedIso.GetRefs();
    REQUIRE (untypedRefs.size() == 1);
    untypedRefs[0].setter (&untypedBootImage);
    CHECK_FALSE (untypedIso.Finalize());

    Image floppyBootImage ("floppy-boot-image");
    REQUIRE (floppyBootImage.Set (ImgProp::PartType, ImageId ("floppy")));
    Image fddIso ("fdd-iso");
    REQUIRE (fddIso.Set (ImgProp::PartType, ImageId ("iso9660")));
    REQUIRE (fddIso.Set (ImgProp::BootEmu, ImageId ("fdd")));
    REQUIRE (fddIso.Set (ImgProp::BootImage, ImageId ("floppy-boot-image")));
    const auto& floppyRefs = fddIso.GetRefs();
    REQUIRE (floppyRefs.size() == 1);
    floppyRefs[0].setter (&floppyBootImage);
    CHECK (fddIso.Finalize());
}

TEST_CASE ("Image component lookup handles invalid slots and null additions")
{
    Image img ("disk1");
    CHECK_FALSE (img.CheckComponent (CompType::Max));
    CHECK_THROWS_AS (img.GetComponent<TestComponent> (CompType::Max), ErrorException);
    CHECK_THROWS_AS (img.AddComponent (nullptr), std::invalid_argument);
}

TEST_CASE ("Partition name-based Get and IsSet report unknown properties")
{
    Partition part ("part0");
    auto get = part.Get<std::string> ("bogus");
    CHECK_FALSE (get);
    CHECK (get.Error().RootFrame().code == ErrorCode::InvalidPartProp);

    auto isSet = part.IsSet ("bogus");
    CHECK_FALSE (isSet);
    CHECK (isSet.Error().RootFrame().code == ErrorCode::InvalidPartProp);

    CHECK_THROWS (part.Set (PartProp::Max, ImageVal (std::string ("x"))));
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
    Image image ("disk1");
    Partition partition ("part1");
    Error err = ImageError::Make (ErrorCode::InvalidImgProp,
        {{"prop", "bogus"}, {"name_suffix", ImageError::NameSuffix (image)}});
    CHECK (ImageError::NameSuffix (partition) == " \"part1\"");
    CHECK (err.RootFrame().msg.find ("disk1") != std::string::npos);
    CHECK (err.RootFrame().msg.find ("bogus") != std::string::npos);
}

TEST_CASE ("ImageError formats an anonymous-image message without a stray name")
{
    Error err =
        ImageError::Make (ErrorCode::InvalidImgProp, {{"prop", "bogus"}, {"name_suffix", ImageError::NameSuffix ("")}});
    CHECK (err.RootFrame().msg.find ("\"\"") == std::string::npos);
}

TEST_CASE ("Error context enriches formatted messages after construction")
{
    Error err = ImageError::Make (ErrorCode::PropTypeMismatch, {{"prop", "boot_mode"}});
    CHECK (err.RootFrame().msg.find ("boot_mode") != std::string::npos);
    CHECK (err.MakeContextStr().empty());

    err.AddContext (SourceLoc ("myfile.conf", 12));
    CHECK (err.MakeContextStr() == "myfile.conf:12: ");
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
    REQUIRE (img.Set (ImgProp::Size, MakeNumId (1, "TiB")));

    constexpr int partCount = 2000;
    for (int i = 0; i < partCount; i++)
    {
        auto part = std::make_unique<Partition> ("part" + std::to_string (i));
        REQUIRE (part->Set ("start", MakeNumId (static_cast<size_t> (i + 1), "MiB")));
        REQUIRE (part->Set ("size", MakeNumId (1, "MiB")));
        img.AddPartition (std::move (part));
    }

    REQUIRE (img.GetPartitions().size() == static_cast<size_t> (partCount));
    for (int i = 0; i < partCount; i++)
    {
        auto& part = img.GetPartitions()[i];
        CHECK (part->GetName() == "part" + std::to_string (i));
        CHECK (*part->Get<uint64_t> ("start").Value() == static_cast<uint64_t> (i + 1) * 1024 * 1024);
    }

    // Repeatedly overwrite the same property many times and make sure the final value sticks
    for (int i = 0; i < 1000; i++)
        REQUIRE (img.Set (ImgProp::BootMode, ImageId (i % 2 == 0 ? "bios" : "efi")));
    auto finalMode = img.Get<BootMode> (ImgProp::BootMode);
    CHECK (*finalMode == BootMode::Efi);
}
