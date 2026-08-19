#include "helper.hpp"

#include <archive_io.hpp>
#include <filesystem.hpp>
#include <path.hpp>

#include <cstdlib>
#include <sstream>

static const char *M = "[archive-cli]";

static Path testPath(const std::string &name)
{
  return Path(REAPACK_TEST_BINARY_DIR) + name;
}

static std::string shellQuote(const std::string &argument)
{
#ifdef _WIN32
  std::string quoted{"\""};
  for(const char c : argument) {
    if(c == '"')
      quoted += '\\';
    quoted += c;
  }
  return quoted + '"';
#else
  std::string quoted{"'"};
  for(const char c : argument) {
    if(c == '\'')
      quoted += "'\\''";
    else
      quoted += c;
  }
  return quoted + '\'';
#endif
}

static const char *silenceOutput()
{
#ifdef _WIN32
  return " >NUL 2>&1";
#else
  return " >/dev/null 2>&1";
#endif
}

static std::string extract(const ArchiveReaderPtr &reader, const Path &path)
{
  std::ostringstream stream;
  REQUIRE(reader->extractFile(path, stream) == 0);
  return stream.str();
}

TEST_CASE("archive CLI help", M) {
  const std::string command = shellQuote(REAPACK_ARCHIVE_EXECUTABLE) +
    " --help" + silenceOutput();
  REQUIRE(std::system(command.c_str()) == 0);
}

TEST_CASE("archive CLI rejects invalid usage", M) {
  const std::string command = shellQuote(REAPACK_ARCHIVE_EXECUTABLE) +
    " --unknown" + silenceOutput();
  REQUIRE(std::system(command.c_str()) != 0);
}

TEST_CASE("build an archive through the CLI", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path inputDir("archive-cli-input");
  const Path index = inputDir + "index.xml";
  const Path payload = inputDir + "payload.lua";
  const Path output = testPath("cli.ReaPackArchive");

  const std::string xml =
    "<index version=\"1\">"
      "<category name=\"Tools\">"
        "<reapack name=\"Payload.lua\" type=\"script\">"
          "<version name=\"1.0\">"
            "<source file=\"payload.lua\" "
              "hash=\"1220239f59ed55e737c77147cf55ad0c1b030b6d7ee748a7426952f9b852d5a935e5\">"
              "payload.lua"
            "</source>"
          "</version>"
        "</reapack>"
      "</category>"
    "</index>";

  UseRootPath useRoot(root);
  FS::remove(output);
  REQUIRE(FS::write(index, xml));
  REQUIRE(FS::write(payload, "payload"));

  const std::string command =
    shellQuote(REAPACK_ARCHIVE_EXECUTABLE) +
    " --index " + shellQuote(FS::canonical(index).join()) +
    " --source-root " + shellQuote(FS::canonical(inputDir).join()) +
    " --repository-name " + shellQuote("CLI Preview") +
    " --output " + shellQuote(output.join()) + silenceOutput();

  REQUIRE(std::system(command.c_str()) == 0);
  REQUIRE(FS::exists(output));

  const auto reader = std::make_shared<ArchiveReader>(output);
  REQUIRE(extract(reader, Path("toc")) ==
    "REPO CLI Preview|https://reapack-preview.invalid/index.xml|0|0\n"
    "PACK \"Tools\" \"Payload.lua\" \"1.0\" 0\n");
  REQUIRE(extract(reader, Path("Scripts/CLI Preview/Tools/payload.lua")) ==
    "payload");

  REQUIRE(FS::remove(index));
  REQUIRE(FS::remove(payload));
  REQUIRE(FS::remove(inputDir));
  REQUIRE(FS::remove(output));
}
