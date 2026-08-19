#include "helper.hpp"

#include <archive_manifest.hpp>
#include <remote.hpp>

#include <sstream>

static const char *M = "[archive-manifest]";

TEST_CASE("write archive manifest records", M) {
  std::ostringstream manifest;

  Archive::writeRepositoryRecord(manifest,
    Remote("Preview", "https://example.invalid/index.xml", false, false));
  const Archive::PackageRecord package{
    "Scripts/Tools", "Preview \"Name\"", "1.0pre1", 3};
  Archive::writePackageRecord(manifest, package);

  REQUIRE(manifest.str() ==
    "REPO Preview|https://example.invalid/index.xml|0|0\n"
    "PACK \"Scripts/Tools\" \"Preview \\\"Name\\\"\" \"1.0pre1\" 3\n");
}

TEST_CASE("parse archive manifest records", M) {
  const Remote repository = Archive::parseRepositoryRecord(
    "Preview|https://example.invalid/index.xml|0|0");
  REQUIRE(repository.name() == "Preview");
  REQUIRE(repository.url() == "https://example.invalid/index.xml");
  REQUIRE_FALSE(repository.isEnabled());
  REQUIRE_FALSE(repository.autoInstall(true));

  const Archive::PackageRecord package = Archive::parsePackageRecord(
    "\"Scripts/Tools\" \"Preview \\\"Name\\\"\" \"1.0pre1\" 3");
  REQUIRE(package.category == "Scripts/Tools");
  REQUIRE(package.package == "Preview \"Name\"");
  REQUIRE(package.version == "1.0pre1");
  REQUIRE(package.flags == 3);
}

TEST_CASE("reject malformed archive manifest records", M) {
  REQUIRE_THROWS_WITH(Archive::parseRepositoryRecord("invalid"),
    "invalid repository record");
  REQUIRE_THROWS_WITH(Archive::parsePackageRecord(
    "\"Scripts/Tools\" \"Preview\""), "invalid package record");
}
