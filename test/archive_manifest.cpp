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
