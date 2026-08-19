#include "helper.hpp"

#include <index.hpp>
#include <errors.hpp>

static const char *M = "[reapack_v1]";
static const Path RIPATH("test/indexes/v1");

TEST_CASE("unnamed category", M) {
  UseRootPath root(RIPATH);

  try {
    IndexPtr ri = Index::load("unnamed_category");
    FAIL();
  }
  catch(const reapack_error &e) {
    REQUIRE(std::string{e.what()} == "empty category name");
  }
}

TEST_CASE("invalid category tag", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("wrong_category_tag");

  REQUIRE(ri->categories().empty());
}

TEST_CASE("invalid package tag", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("wrong_package_tag");
  REQUIRE(ri->categories().empty());

}

TEST_CASE("null package name", M) {
  UseRootPath root(RIPATH);

  try {
    IndexPtr ri = Index::load("unnamed_package");
    FAIL();
  }
  catch(const reapack_error &e) {
    REQUIRE(std::string{e.what()} == "empty package name");
  }
}

TEST_CASE("null package type", M) {
  UseRootPath root(RIPATH);

  SECTION("compatible loading tolerates it") {
    IndexPtr ri = Index::load("missing_type");
    REQUIRE(ri->packages().empty());
  }

  SECTION("strict loading rejects it") {
    Index::LoadOptions options;
    options.rejectUnsupportedPackageTypes = true;
    REQUIRE_THROWS_WITH(Index::load("missing_type", nullptr, options),
      "missing package type");
  }
}

TEST_CASE("unsupported package type", M) {
  UseRootPath root(RIPATH);

  SECTION("compatible loading ignores it") {
    IndexPtr ri = Index::load("unsupported_type");
    REQUIRE(ri->packages().empty());
  }

  SECTION("strict loading rejects it") {
    Index::LoadOptions options;
    options.rejectUnsupportedPackageTypes = true;
    REQUIRE_THROWS_WITH(Index::load("unsupported_type", nullptr, options),
      "unsupported package type 'hello_world'");
  }
}

TEST_CASE("read version author", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("author");

  CHECK(ri->packages().size() == 1);
  REQUIRE(ri->category(0)->package(0)->version(0)->author()
    == "Watanabe Saki");
}

TEST_CASE("read version time", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("time");
  CHECK(ri->packages().size() == 1);

  const Time &time = ri->category(0)->package(0)->version(0)->time();
  REQUIRE(time.year() == 2016);
  REQUIRE(time.month() == 2);
  REQUIRE(time.day() == 12);
}

TEST_CASE("invalid version tag", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("wrong_version_tag");
  REQUIRE(ri->categories().empty());
}

TEST_CASE("null package version", M) {
  UseRootPath root(RIPATH);

  try {
    IndexPtr ri = Index::load("missing_version");
    FAIL();
  }
  catch(const reapack_error &e) {
    REQUIRE(std::string{e.what()} == "invalid version name ''");
  }
}

TEST_CASE("null source url", M) {
  UseRootPath root(RIPATH);

  try {
    IndexPtr ri = Index::load("missing_source_url");
    FAIL();
  }
  catch(const reapack_error &e) {
    REQUIRE(std::string{e.what()} == "empty source url");
  }
}

TEST_CASE("null source file", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("missing_source_file");
  CHECK(ri->packages().size() == 1);

  const Package *pkg = ri->category(0)->package(0);
  REQUIRE(pkg->version(0)->source(0)->file() == pkg->name());
}

TEST_CASE("default source platform", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("missing_platform");

  CHECK(ri->packages().size() == 1);
  REQUIRE(ri->category(0)->package(0)->version(0)->source(0)->platform()
    == Platform::Generic);
}

TEST_CASE("version changelog", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("changelog");
  CHECK(ri->packages().size() == 1);

  REQUIRE(ri->category(0)->package(0)->version(0)->changelog()
    == "Hello\nWorld");
}

TEST_CASE("full index", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("valid_index");

  REQUIRE(ri->categories().size() == 1);
  const Category *cat = ri->category(0);
  REQUIRE(cat->name() == "Category Name");

  REQUIRE(cat->packages().size() == 1);
  const Package *pack = cat->package(0);
  REQUIRE(pack->type() == Package::ScriptType);
  REQUIRE(pack->name() == "Hello World.lua");

  REQUIRE(pack->versions().size() == 1);
  const Version *ver = pack->version(0);
  REQUIRE(ver->name() == VersionName("1.0"));
  REQUIRE(ver->changelog() == "Fixed a division by zero error.");

  REQUIRE(ver->sources().size() == 2);
  const Source *source1 = ver->source(0);
  REQUIRE(source1->platform() == Platform::Generic);
  REQUIRE(source1->file() == "test.lua");
  REQUIRE(source1->sections() == Source::MainSection);
  REQUIRE(source1->url() == "https://google.com/");

  const Source *source2 = ver->source(1);
  REQUIRE(source2->platform() == Platform::Generic);
  REQUIRE(source2->file() == "background.png");
  REQUIRE_FALSE(source2->sections() == Source::MainSection);
  REQUIRE(source2->url() == "http://cfillion.tk/");
}

TEST_CASE("read index metadata", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("metadata");

  SECTION("name (ignored)") {
    REQUIRE(ri->name() == "metadata");
  }

  SECTION("description") {
    REQUIRE(ri->metadata()->about() == "Chunky\nBacon");
  }

  SECTION("links") {
    REQUIRE(ri->metadata()->links().size() == 5);

    auto link = ri->metadata()->links().begin();
    REQUIRE(link->first == Metadata::WebsiteLink);
    REQUIRE(link->second.name == "http://cfillion.tk");
    REQUIRE(link->second.url == "http://cfillion.tk");

    REQUIRE((++link)->second.name == "https://github.com/cfillion");
    REQUIRE(link->second.url == "https://github.com/cfillion");

    REQUIRE((++link)->second.name == "http://twitter.com/cfi30");
    REQUIRE(link->second.url == "http://twitter.com/cfi30");

    REQUIRE((++link)->second.name == "http://google.com");
    REQUIRE(link->second.url == "http://google.com");

    REQUIRE((++link)->first == Metadata::DonationLink);
    REQUIRE(link->second.name == "Donate");
    REQUIRE(link->second.url == "http://paypal.com");
  }
}

TEST_CASE("read package metadata", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("pkg_metadata");
  REQUIRE(ri->packages().size() == 1);

  const Package *pkg = ri->packages()[0];

  SECTION("description") {
    REQUIRE(pkg->metadata()->about() == "Chunky\nBacon");
  }

  SECTION("links") {
    REQUIRE(pkg->metadata()->links().size() == 2);

    auto link = pkg->metadata()->links().begin();
    REQUIRE(link->first == Metadata::WebsiteLink);
    REQUIRE(link->second.name == "http://cfillion.tk");
    REQUIRE(link->second.url == "http://cfillion.tk");

    REQUIRE((++link)->first == Metadata::DonationLink);
    REQUIRE(link->second.name == "Donate");
    REQUIRE(link->second.url == "http://paypal.com");
  }
}

TEST_CASE("read index name (from raw data only)") {
  SECTION("valid") {
    IndexPtr ri = Index::load({}, "<index version=\"1\" name=\"Hello World\"/>\n");
    REQUIRE(ri->name() == "Hello World");
  }

  SECTION("missing") {
    IndexPtr ri = Index::load({}, "<index version=\"1\"/>\n");
    REQUIRE(ri->name() == "");
  }
}

TEST_CASE("read source platform", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("src_platform");

  REQUIRE(ri->packages().size() == 1);

#ifdef __APPLE__
  const auto expected = Platform::Darwin_Any;
#elif _WIN32
  const auto expected = Platform::Windows_Any;
#elif __linux__
  const auto expected = Platform::Linux_Any;
#endif

  REQUIRE(ri->category(0)->package(0)->version(0)->source(0)->platform()
    == expected);
}

TEST_CASE("unsupported source platform", M) {
  const char *data =
    "<index version=\"1\">"
      "<category name=\"test\">"
        "<reapack name=\"a\" type=\"script\">"
          "<version name=\"1.0\">"
            "<source platform=\"future\">a.lua</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  SECTION("compatible loading ignores it") {
    IndexPtr ri = Index::load({}, data);
    REQUIRE(ri->packages().empty());
  }

  SECTION("strict loading rejects it") {
    Index::LoadOptions options;
    options.rejectUnsupportedPlatforms = true;
    REQUIRE_THROWS_WITH(Index::load({}, data, options),
      "unsupported source platform 'future'");
  }
}

TEST_CASE("read source type override", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("src_type");

  CHECK(ri->packages().size() == 1);
  REQUIRE(ri->category(0)->package(0)->version(0)->source(0)->typeOverride()
    == Package::EffectType);
}

TEST_CASE("unsupported source type override", M) {
  const char *data =
    "<index version=\"1\">"
      "<category name=\"test\">"
        "<reapack name=\"a\" type=\"script\">"
          "<version name=\"1.0\">"
            "<source type=\"future\">a.lua</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  SECTION("compatible loading treats it as inherited") {
    IndexPtr ri = Index::load({}, data);
    REQUIRE(ri->packages().size() == 1);
    REQUIRE(ri->packages()[0]->version(0)->source(0)->typeOverride()
      == Package::UnknownType);
    REQUIRE(ri->packages()[0]->version(0)->source(0)->type()
      == Package::ScriptType);
  }

  SECTION("strict loading rejects it") {
    Index::LoadOptions options;
    options.rejectUnsupportedSourceTypes = true;
    REQUIRE_THROWS_WITH(Index::load({}, data, options),
      "unsupported source type 'future'");
  }
}

TEST_CASE("duplicate source target", M) {
  const char *data =
    "<index version=\"1\">"
      "<category name=\"test\">"
        "<reapack name=\"a\" type=\"data\">"
          "<version name=\"1.0\">"
            "<source file=\"shared.dat\">first.dat</source>"
            "<source file=\"shared.dat\">second.dat</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  SECTION("compatible loading keeps the first source") {
    IndexPtr ri = Index::load({}, data);
    REQUIRE(ri->packages().size() == 1);
    REQUIRE(ri->packages()[0]->version(0)->sources().size() == 1);
    REQUIRE(ri->packages()[0]->version(0)->source(0)->url() == "first.dat");
  }

  SECTION("strict loading rejects it") {
    Index::LoadOptions options;
    options.rejectDuplicateSourceTargets = true;
    REQUIRE_THROWS_WITH(Index::load({}, data, options),
      "duplicate source target 'Data/shared.dat'");
  }
}

TEST_CASE("read package description", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("pkg_desc");

  CHECK(ri->packages().size() == 1);
  REQUIRE(ri->category(0)->package(0)->description() == "From the New World");
}

TEST_CASE("read multiple sections", M) {
  UseRootPath root(RIPATH);

  IndexPtr ri = Index::load("explicit_sections");

  CHECK(ri->packages().size() == 1);
  REQUIRE(ri->category(0)->package(0)->version(0)->source(0)->sections()
    == (Source::MainSection | Source::MIDIEditorSection));
}

TEST_CASE("read sha256 checksum", M) {
  IndexPtr ri = Index::load({}, R"(
<index version="1">
  <category name="catname">
    <reapack name="packname" type="script">
      <version name="1.0" author="John Doe">
        <source hash="12206037d8b51b33934348a2b26e04f0eb7227315b87bb5688ceb6dccb0468b14cce">https://google.com/</source>
      </version>
    </reapack>
  </category>
</index>
  )");

  REQUIRE(ri->packages().size() == 1);

  REQUIRE(ri->category(0)->package(0)->version(0)->source(0)->checksum()
    == "12206037d8b51b33934348a2b26e04f0eb7227315b87bb5688ceb6dccb0468b14cce");
}
