#include "helper.hpp"

#include <archive_builder.hpp>
#include <archive_io.hpp>
#include <errors.hpp>
#include <filesystem.hpp>
#include <path.hpp>

#include <sstream>

static const char *M = "[archive-builder]";

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

TEST_CASE("build a ReaPack archive from an explicit plan", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-builder-input");
  const Path index = inputDir + "index.xml";
  const Path script = inputDir + "preview.lua";
  const Path data = inputDir + "preview.dat";
  const Path output = testPath("preview.ReaPackArchive");
  const std::string indexContents = "<index version=\"1\" name=\"Preview\"/>\n";
  const std::string scriptContents = "return 'preview'\n";
  const std::string dataContents{"\x00\x01\xff\x00", 4};

  UseRootPath useRoot(root);
  FS::remove(output);
  FS::remove(index);
  FS::remove(script);
  FS::remove(data);

  REQUIRE(FS::write(index, indexContents));
  REQUIRE(FS::write(script, scriptContents));
  REQUIRE(FS::write(data, dataContents));

  Archive::BuildPlan plan;
  plan.repository = Remote("Project Preview - PR 123",
    "https://example.invalid/index.xml", false, false);
  plan.index = index;
  plan.packages.push_back({"Scripts/Project", "Preview Tools", "1.2.3", 0});
  plan.packages.push_back({"Data/Project", "Preview Data", "2.0pre1", 1});
  plan.payloads.push_back({script, "Scripts/Project/preview.lua"});
  plan.payloads.push_back({data, "Data/Project/preview.dat"});

  Archive::build(plan, output);

  REQUIRE(FS::exists(output));
  REQUIRE_FALSE(FS::exists(Path(output.join() + ".part")));

  const auto reader = std::make_shared<ArchiveReader>(output);
  REQUIRE(extract(reader, Path("toc")) ==
    "REPO Project Preview - PR 123|https://example.invalid/index.xml|0|0\n"
    "PACK \"Scripts/Project\" \"Preview Tools\" \"1.2.3\" 0\n"
    "PACK \"Data/Project\" \"Preview Data\" \"2.0pre1\" 1\n");
  REQUIRE(extract(reader,
    Path("ReaPack/cache/Project Preview - PR 123.xml")) == indexContents);
  REQUIRE(extract(reader, Path("Scripts/Project/preview.lua")) == scriptContents);
  REQUIRE(extract(reader, Path("Data/Project/preview.dat")) == dataContents);

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(script));
  REQUIRE(FS::remove(data));
  REQUIRE(FS::remove(inputDir));
  REQUIRE(FS::remove(output));
}

TEST_CASE("reject unsafe or duplicate archive targets", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-builder-validation");
  const Path index = inputDir + "index.xml";
  const Path payload = inputDir + "payload";
  const Path output = testPath("invalid.ReaPackArchive");

  UseRootPath useRoot(root);
  FS::remove(output);
  REQUIRE(FS::write(index, "<index/>"));
  REQUIRE(FS::write(payload, "payload"));

  Archive::BuildPlan plan;
  plan.repository = Remote("Preview", "https://example.invalid/index.xml");
  plan.index = index;

  SECTION("parent traversal")
    plan.payloads.push_back({payload, "Scripts/../escape"});
  SECTION("absolute path")
    plan.payloads.push_back({payload, "/absolute"});
  SECTION("Windows path")
    plan.payloads.push_back({payload, "C:/absolute"});
  SECTION("reserved table of contents")
    plan.payloads.push_back({payload, "toc"});
  SECTION("duplicate target") {
    plan.payloads.push_back({payload, "Scripts/Preview.lua"});
    plan.payloads.push_back({payload, "scripts/preview.lua"});
  }

  REQUIRE_THROWS_AS(Archive::build(plan, output), reapack_error);
  REQUIRE_FALSE(FS::exists(output));
  REQUIRE_FALSE(FS::exists(Path(output.join() + ".part")));

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(payload));
  REQUIRE(FS::remove(inputDir));
}

TEST_CASE("validate archive inputs before writing", M) {
  const Path output = testPath("missing-input.ReaPackArchive");
  Archive::BuildPlan plan;
  plan.repository = Remote("Preview", "https://example.invalid/index.xml");
  plan.index = testPath("missing-index.xml");

  FS::remove(output);
  REQUIRE_THROWS_WITH(Archive::build(plan, output),
    "index file not found: " + plan.index.join());
  REQUIRE_FALSE(FS::exists(output));
  REQUIRE_FALSE(FS::exists(Path(output.join() + ".part")));
}

TEST_CASE("validate payload checksums before writing", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-builder-checksum");
  const Path index = inputDir + "index.xml";
  const Path payload = inputDir + "payload";
  const Path output = testPath("checksum.ReaPackArchive");

  UseRootPath useRoot(root);
  FS::remove(output);
  REQUIRE(FS::write(index, "<index/>"));
  REQUIRE(FS::write(payload, "payload"));

  Archive::BuildPlan plan;
  plan.repository = Remote("Preview", "https://example.invalid/index.xml");
  plan.index = index;
  plan.payloads.push_back({payload, "Data/payload",
    "1220239f59ed55e737c77147cf55ad0c1b030b6d7ee748a7426952f9b852d5a935e5"});

  SECTION("matching") {
    Archive::build(plan, output);
    REQUIRE(FS::exists(output));
  }

  SECTION("mismatch") {
    plan.payloads[0].checksum.back() = '0';
    REQUIRE_THROWS_WITH(Archive::build(plan, output),
      "checksum mismatch for " + payload.join());
    REQUIRE_FALSE(FS::exists(output));
  }

  SECTION("malformed") {
    plan.payloads[0].checksum = "sha256:invalid";
    REQUIRE_THROWS_WITH(Archive::build(plan, output),
      "invalid checksum for " + payload.join());
    REQUIRE_FALSE(FS::exists(output));
  }

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(payload));
  REQUIRE(FS::remove(inputDir));
  if(FS::exists(output))
    REQUIRE(FS::remove(output));
}
