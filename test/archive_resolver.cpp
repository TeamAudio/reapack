#include "helper.hpp"

#include <archive_builder.hpp>
#include <archive_io.hpp>
#include <archive_resolver.hpp>
#include <errors.hpp>
#include <filesystem.hpp>
#include <path.hpp>

#include <sstream>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

static const char *M = "[archive-resolver]";

static Path testPath(const std::string &name)
{
  return Path(REAPACK_TEST_BINARY_DIR) + name;
}

static std::string extract(const ArchiveReaderPtr &reader, const Path &path)
{
  std::ostringstream stream;
  REQUIRE(reader->extractFile(path, stream) == 0);
  return stream.str();
}

TEST_CASE("resolve sources for the current platform", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-resolver-input");
  const Path index = inputDir + "index.xml";
  const Path generic = inputDir + "generic.lua";
  const Path current = inputDir + "current.lua";
  const Path output = testPath("resolved.ReaPackArchive");
  const std::string urlPrefix = "https://preview.invalid/files/";

#ifdef __APPLE__
  const char *currentPlatform = "darwin";
  const char *foreignPlatform = "windows";
#elif defined(__linux__)
  const char *currentPlatform = "linux";
  const char *foreignPlatform = "darwin";
#else
  const char *currentPlatform = "windows";
  const char *foreignPlatform = "linux";
#endif

  std::ostringstream xml;
  xml <<
    "<index version=\"1\" name=\"Ignored index name\">"
      "<category name=\"Tools\">"
        "<reapack name=\"Preview.lua\" type=\"script\">"
          "<version name=\"1.0\">"
            "<source platform=\"all\" file=\"generic.lua\" "
              "hash=\"12203a2e8954befdbd6e7eac2f10d4301a2923cd65a5f38bf80914019b55a03f78c4\">"
              << urlPrefix << "generic.lua</source>"
            "<source platform=\"" << currentPlatform <<
              "\" file=\"current.lua\" "
              "hash=\"122097b0560280ed60a5a1eaa1bc45492543c8a986ad5a25b468c427eb83c3e88191\">"
              << urlPrefix << "current.lua</source>"
            "<source platform=\"" << foreignPlatform <<
              "\" file=\"foreign.lua\">" << urlPrefix << "foreign.lua</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  UseRootPath useRoot(root);
  FS::remove(output);
  REQUIRE(FS::write(index, xml.str()));
  REQUIRE(FS::write(generic, "generic"));
  REQUIRE(FS::write(current, "current"));

  std::vector<std::string> located;
  const Archive::SourceLocator locate =
    Archive::makeSourceRootLocator(inputDir, urlPrefix);
  Archive::RepositoryInput input;
  input.repository = Remote("Preview", "https://example.invalid/index.xml",
    false, false);
  input.index = index;
  input.locateSource = [&](const std::string &url) {
    located.push_back(url);
    return locate(url);
  };

  const Archive::BuildPlan plan = Archive::resolve(input);

  REQUIRE(plan.repository.name() == "Preview");
  REQUIRE(plan.index == index);
  REQUIRE(plan.packages.size() == 1);
  REQUIRE(plan.packages[0].category == "Tools");
  REQUIRE(plan.packages[0].package == "Preview.lua");
  REQUIRE(plan.packages[0].version == "1.0");
  REQUIRE(plan.payloads.size() == 2);
  REQUIRE(located == std::vector<std::string>{
    urlPrefix + "generic.lua", urlPrefix + "current.lua"});
  REQUIRE(plan.payloads[0].checksum ==
    "12203a2e8954befdbd6e7eac2f10d4301a2923cd65a5f38bf80914019b55a03f78c4");

  Archive::build(plan, output);

  const auto reader = std::make_shared<ArchiveReader>(output);
  REQUIRE(extract(reader, Path("toc")) ==
    "REPO Preview|https://example.invalid/index.xml|0|0\n"
    "PACK \"Tools\" \"Preview.lua\" \"1.0\" 0\n");
  REQUIRE(extract(reader, Path("Scripts/Preview/Tools/generic.lua")) == "generic");
  REQUIRE(extract(reader, Path("Scripts/Preview/Tools/current.lua")) == "current");

  std::ostringstream missing;
  REQUIRE(reader->extractFile(Path("Scripts/Preview/Tools/foreign.lua"),
    missing) != 0);

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(generic));
  REQUIRE(FS::remove(current));
  REQUIRE(FS::remove(inputDir));
  REQUIRE(FS::remove(output));
}

TEST_CASE("locate URL paths beneath a source root", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-source-root");
  const Path source = inputDir + "space name.lua";

  UseRootPath useRoot(root);
  REQUIRE(FS::write(source, "source"));

  SECTION("relative URL") {
    const auto locate = Archive::makeSourceRootLocator(inputDir);
    REQUIRE(locate("space%20name.lua") == FS::canonical(source));
  }

  SECTION("current directory source root") {
    UseRootPath useInputDir(FS::canonical(inputDir));
    const auto locate = Archive::makeSourceRootLocator(Path("."));
    REQUIRE(locate("space%20name.lua") == FS::canonical(Path("space name.lua")));
  }

  SECTION("absolute URL with prefix") {
    const auto locate = Archive::makeSourceRootLocator(inputDir,
      "https://preview.invalid/files/");
    REQUIRE(locate("https://preview.invalid/files/space%20name.lua") ==
      FS::canonical(source));
  }

  SECTION("absolute URL without prefix") {
    const auto locate = Archive::makeSourceRootLocator(inputDir);
    REQUIRE_THROWS_AS(locate("https://preview.invalid/files/source.lua"),
      reapack_error);
  }

  SECTION("URL outside prefix") {
    const auto locate = Archive::makeSourceRootLocator(inputDir,
      "https://preview.invalid/files/");
    REQUIRE_THROWS_AS(locate("https://other.invalid/files/source.lua"),
      reapack_error);
  }

  SECTION("parent traversal") {
    const auto locate = Archive::makeSourceRootLocator(inputDir);
    REQUIRE_THROWS_AS(locate("../source.lua"), reapack_error);
    REQUIRE_THROWS_AS(locate("%2e%2e/source.lua"), reapack_error);
  }

#ifndef _WIN32
  SECTION("symlink escape") {
    const Path outside("archive-source-outside.lua");
    const Path link = inputDir + "escape.lua";
    REQUIRE(FS::write(outside, "outside"));
    REQUIRE(symlink("../archive-source-outside.lua",
      (root + link).join().c_str()) == 0);

    const auto locate = Archive::makeSourceRootLocator(inputDir);
    REQUIRE_THROWS_AS(locate("escape.lua"), reapack_error);

    REQUIRE(FS::remove(link));
    REQUIRE(FS::remove(outside));
  }
#endif

  SECTION("URL query") {
    const auto locate = Archive::makeSourceRootLocator(inputDir);
    REQUIRE_THROWS_AS(locate("source.lua?download=1"), reapack_error);
  }

  REQUIRE(FS::remove(source));
  REQUIRE(FS::remove(inputDir));
}

TEST_CASE("reject unsupported package types", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-unsupported-package");
  const Path index = inputDir + "index.xml";
  const std::string xml =
    "<index version=\"1\">"
      "<category name=\"Tools\">"
        "<reapack name=\"Preview\" type=\"future\">"
          "<version name=\"1.0\">"
            "<source>preview.dat</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  UseRootPath useRoot(root);
  REQUIRE(FS::write(index, xml));

  Archive::RepositoryInput input;
  input.repository = Remote("Preview", "https://example.invalid/index.xml");
  input.index = index;
  input.locateSource = [](const std::string &) { return Path(); };

  REQUIRE_THROWS_WITH(Archive::resolve(input),
    "unsupported package type 'future'");

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(inputDir));
}

TEST_CASE("reject unsupported source platforms", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-unsupported-platform");
  const Path index = inputDir + "index.xml";
  const std::string xml =
    "<index version=\"1\">"
      "<category name=\"Tools\">"
        "<reapack name=\"Preview\" type=\"script\">"
          "<version name=\"1.0\">"
            "<source platform=\"future\">preview.lua</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  UseRootPath useRoot(root);
  REQUIRE(FS::write(index, xml));

  Archive::RepositoryInput input;
  input.repository = Remote("Preview", "https://example.invalid/index.xml");
  input.index = index;
  input.locateSource = [](const std::string &) { return Path(); };

  REQUIRE_THROWS_WITH(Archive::resolve(input),
    "unsupported source platform 'future'");

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(inputDir));
}

TEST_CASE("reject unsupported source type overrides", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-unsupported-source-type");
  const Path index = inputDir + "index.xml";
  const std::string xml =
    "<index version=\"1\">"
      "<category name=\"Tools\">"
        "<reapack name=\"Preview\" type=\"script\">"
          "<version name=\"1.0\">"
            "<source type=\"future\">preview.lua</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  UseRootPath useRoot(root);
  REQUIRE(FS::write(index, xml));

  Archive::RepositoryInput input;
  input.repository = Remote("Preview", "https://example.invalid/index.xml");
  input.index = index;
  input.locateSource = [](const std::string &) { return Path(); };

  REQUIRE_THROWS_WITH(Archive::resolve(input),
    "unsupported source type 'future'");

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(inputDir));
}
