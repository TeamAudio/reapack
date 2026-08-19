#include "helper.hpp"
#include "http_server.hpp"

#include <archive_source.hpp>
#include <filesystem.hpp>
#include <path.hpp>
#include <transfer.hpp>

#include <fstream>
#include <sstream>
#include <string>

static const char *M = "[transfer]";

class TransferRuntime {
public:
  TransferRuntime() { Transfer::globalInit(); }
  ~TransferRuntime() { Transfer::globalCleanup(); }
};

static Transfer::Result perform(const std::string &url,
  std::ostringstream &output, const std::string &checksum = {})
{
  Transfer::Request request;
  request.url = url;
  request.userAgent = "ReaPack transfer test";
  request.expectedChecksum = checksum;
  return Transfer::perform(request, output);
}

static std::string readFile(const Path &path)
{
  std::ifstream stream;
  REQUIRE(FS::open(stream, path));

  std::ostringstream contents;
  contents << stream.rdbuf();
  return contents.str();
}

TEST_CASE("download an HTTP response with its checksum", M) {
  TransferRuntime transferRuntime;
  HttpServer server(httpResponse("200 OK", "payload"));
  std::ostringstream output;

  const Transfer::Result result = perform(server.url("/payload"), output,
    "1220239f59ed55e737c77147cf55ad0c1b030b6d7ee748a7426952f9b852d5a935e5");

  REQUIRE(static_cast<bool>(result));
  REQUIRE(result.status == 200);
  REQUIRE(output.str() == "payload");
}

TEST_CASE("follow an HTTP redirect", M) {
  TransferRuntime transferRuntime;
  HttpServer destination(httpResponse("200 OK", "redirected"));
  HttpServer redirect(httpResponse("302 Found", "",
    "Location: " + destination.url("/destination") + "\r\n"));
  std::ostringstream output;

  const Transfer::Result result = perform(redirect.url("/redirect"), output);

  REQUIRE(static_cast<bool>(result));
  REQUIRE(result.status == 200);
  REQUIRE(output.str() == "redirected");
}

TEST_CASE("report HTTP and checksum failures", M) {
  TransferRuntime transferRuntime;
  std::ostringstream output;
  Transfer::Result result;

  SECTION("HTTP status") {
    HttpServer server(httpResponse("404 Not Found", "missing"));
    result = perform(server.url("/missing"), output);

    REQUIRE(result.failure == Transfer::Failure::Transport);
    REQUIRE(result.status == 404);
  }

  SECTION("checksum mismatch") {
    HttpServer server(httpResponse("200 OK", "payload"));
    result = perform(server.url("/payload"), output,
      "1220039f59ed55e737c77147cf55ad0c1b030b6d7ee748a7426952f9b852d5a935e5");

    REQUIRE(result.failure == Transfer::Failure::Checksum);
    REQUIRE(result.status == 200);
  }

  REQUIRE_FALSE(static_cast<bool>(result));
  REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("restrict a transfer to HTTP protocols", M) {
  TransferRuntime transferRuntime;
  Transfer::Request request;
  request.url = "ftp://127.0.0.1/source";
  request.httpOnly = true;
  std::ostringstream output;

  const Transfer::Result result = Transfer::perform(request, output);

  REQUIRE(result.failure == Transfer::Failure::Transport);
  REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("prefer local sources while acquiring an archive", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path sourceRoot("archive-source-acquire-local");
  const Path source = sourceRoot + "source.lua";
  const Path stagingRoot("archive-source-acquire-local-staging");

  UseRootPath useRoot(root);
  FS::remove(stagingRoot + "source-0");
  FS::remove(stagingRoot);
  FS::remove(source);
  FS::remove(sourceRoot);
  REQUIRE(FS::write(source, "local"));

  Archive::SourceAcquireOptions options;
  options.sourceRoot = sourceRoot;
  options.sourceUrlPrefix = "https://private.invalid/sources/";
  options.stagingRoot = stagingRoot;

  {
    Archive::SourceAcquirer acquirer(options);
    const Archive::SourceLocator locate = acquirer.locator();

    REQUIRE(locate("source.lua", {}) == FS::canonical(source));
    REQUIRE(locate("https://private.invalid/sources/source.lua", {}) ==
      FS::canonical(source));
    REQUIRE_FALSE(FS::exists(stagingRoot, true));
  }

  REQUIRE_FALSE(FS::exists(stagingRoot, true));
  REQUIRE(FS::remove(source));
  REQUIRE(FS::remove(sourceRoot));
}

TEST_CASE("download and clean up an external archive source", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path sourceRoot("archive-source-acquire-remote");
  const Path stagingRoot("archive-source-acquire-remote-staging");
  Path downloaded;

  UseRootPath useRoot(root);
  FS::remove(stagingRoot + "source-0");
  FS::remove(stagingRoot);
  FS::remove(sourceRoot);
  REQUIRE(FS::mkdir(sourceRoot));

  Archive::SourceAcquireOptions options;
  options.sourceRoot = sourceRoot;
  options.sourceUrlPrefix = "https://private.invalid/sources/";
  options.stagingRoot = stagingRoot;

  {
    Archive::SourceAcquirer acquirer(options);
    HttpServer server(httpResponse("200 OK", "external"));
    const Archive::SourceLocator locate = acquirer.locator();
    const std::string checksum =
      "12203c4623849a49a53911c4a3e48d8cead8a1858960bccdea7a1b978d73ec2f06d7";

    downloaded = locate(server.url("/external"), checksum);
    REQUIRE(FS::exists(downloaded));
    REQUIRE(readFile(downloaded) == "external");
    REQUIRE(locate(server.url("/external"), checksum) == downloaded);
  }

  REQUIRE_FALSE(FS::exists(downloaded));
  REQUIRE_FALSE(FS::exists(stagingRoot, true));
  REQUIRE(FS::remove(sourceRoot));
}

TEST_CASE("reject remote archive sources in offline mode", M) {
  const Path root(REAPACK_TEST_BINARY_DIR);
  const Path sourceRoot("archive-source-acquire-offline");
  const Path stagingRoot("archive-source-acquire-offline-staging");

  UseRootPath useRoot(root);
  FS::remove(stagingRoot + "source-0");
  FS::remove(stagingRoot);
  FS::remove(sourceRoot);
  REQUIRE(FS::mkdir(sourceRoot));

  Archive::SourceAcquireOptions options;
  options.sourceRoot = sourceRoot;
  options.stagingRoot = stagingRoot;
  options.offline = true;

  {
    Archive::SourceAcquirer acquirer(options);
    const Archive::SourceLocator locate = acquirer.locator();

    REQUIRE_THROWS_WITH(locate("https://example.invalid/source.lua", {}),
      "remote source unavailable in offline mode: "
      "https://example.invalid/source.lua");
    REQUIRE_THROWS_WITH(locate("ftp://example.invalid/source.lua", {}),
      "unsupported source URL scheme 'ftp'");
  }

  REQUIRE_FALSE(FS::exists(stagingRoot, true));
  REQUIRE(FS::remove(sourceRoot));
}
