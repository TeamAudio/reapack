#include "helper.hpp"

#include <archive_io.hpp>
#include <filesystem.hpp>
#include <path.hpp>

#include <fstream>
#include <sstream>

static const char *M = "[archive]";

static Path testPath(const std::string &name)
{
  return Path(REAPACK_TEST_BINARY_DIR) + name;
}

TEST_CASE("archive stream round trip", M) {
  const Path archive = testPath("stream-round-trip.ReaPackArchive");
  FS::remove(archive);

  const std::string text = "local repository payload\nwith another line\n";
  const std::string binary{"\x00\x01\x02\xff\x00", 5};

  {
    ArchiveWriter writer(archive);

    std::istringstream textStream(text);
    REQUIRE(writer.addFile(Path("Scripts/example.lua"), textStream) == 0);

    std::istringstream binaryStream(binary);
    REQUIRE(writer.addFile(Path("Data/binary.dat"), binaryStream) == 0);

    std::istringstream emptyStream;
    REQUIRE(writer.addFile(Path("empty"), emptyStream) == 0);
  }

  REQUIRE(FS::exists(archive));

  {
    ArchiveReader reader(archive);

    std::ostringstream textStream;
    REQUIRE(reader.extractFile(Path("Scripts/example.lua"), textStream) == 0);
    REQUIRE(textStream.str() == text);

    std::ostringstream binaryStream;
    REQUIRE(reader.extractFile(Path("Data/binary.dat"), binaryStream) == 0);
    REQUIRE(binaryStream.str() == binary);

    std::ostringstream emptyStream;
    REQUIRE(reader.extractFile(Path("empty"), emptyStream) == 0);
    REQUIRE(emptyStream.str().empty());

    std::ostringstream missingStream;
    REQUIRE(reader.extractFile(Path("missing"), missingStream) != 0);
    REQUIRE(missingStream.str().empty());
  }

  REQUIRE(FS::remove(archive));
}

TEST_CASE("archive filesystem round trip", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path archive = testPath("filesystem-round-trip.ReaPackArchive");
  const Path payload("Scripts/Company/example.lua");
  const std::string contents = "return 'from disk'\n";

  UseRootPath useRoot(root);
  FS::remove(archive);
  FS::removeRecursive(payload);

  REQUIRE(FS::write(payload, contents));
  REQUIRE(FS::exists(payload));

  {
    ArchiveWriter writer(archive);
    REQUIRE(writer.addFile(payload) == 0);
  }

  REQUIRE(FS::remove(payload));
  REQUIRE_FALSE(FS::exists(payload));

  {
    ArchiveReader reader(archive);
    REQUIRE(reader.extractFile(payload) == 0);
  }

  REQUIRE(FS::exists(payload));

  {
    std::ifstream restored;
    REQUIRE(FS::open(restored, payload));
    std::ostringstream restoredContents;
    restoredContents << restored.rdbuf();
    REQUIRE(restoredContents.str() == contents);
  }

  REQUIRE(FS::removeRecursive(payload));
  REQUIRE(FS::remove(Path("Scripts")));
  REQUIRE(FS::remove(archive));
}

TEST_CASE("archive writer replaces an existing archive", M) {
  const Path archive = testPath("replace-existing.ReaPackArchive");
  FS::remove(archive);

  {
    ArchiveWriter writer(archive);
    std::istringstream oldContents("old");
    REQUIRE(writer.addFile(Path("old"), oldContents) == 0);
  }

  {
    ArchiveWriter writer(archive);
    std::istringstream newContents("new");
    REQUIRE(writer.addFile(Path("new"), newContents) == 0);
  }

  {
    ArchiveReader reader(archive);

    std::ostringstream oldContents;
    REQUIRE(reader.extractFile(Path("old"), oldContents) != 0);

    std::ostringstream newContents;
    REQUIRE(reader.extractFile(Path("new"), newContents) == 0);
    REQUIRE(newContents.str() == "new");
  }

  REQUIRE(FS::remove(archive));
}
