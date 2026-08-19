/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2025  Christian Fillion
 * Copyright (C) 2026       Michael DeFreitas
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "archive_resolver.hpp"

#include "errors.hpp"
#include "filesystem.hpp"
#include "index.hpp"
#include "package.hpp"
#include "source.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

static int hexValue(const char digit)
{
  if(digit >= '0' && digit <= '9')
    return digit - '0';
  if(digit >= 'a' && digit <= 'f')
    return digit - 'a' + 10;
  if(digit >= 'A' && digit <= 'F')
    return digit - 'A' + 10;
  return -1;
}

static std::string decodeUrlPath(const std::string &path)
{
  std::string decoded;
  decoded.reserve(path.size());

  for(size_t i = 0; i < path.size(); ++i) {
    if(path[i] != '%') {
      decoded += path[i];
      continue;
    }

    if(i + 2 >= path.size())
      throw reapack_error("invalid percent escape in source URL");

    const int high = hexValue(path[i + 1]);
    const int low = hexValue(path[i + 2]);
    if(high < 0 || low < 0)
      throw reapack_error("invalid percent escape in source URL");

    decoded += static_cast<char>((high << 4) | low);
    i += 2;
  }

  return decoded;
}

static void validateSourcePath(const std::string &path)
{
  if(path.empty() || path.front() == '/' || path.find('\\') != std::string::npos
      || path.find(':') != std::string::npos
      || path.find_first_of("?#") != std::string::npos) {
    throw reapack_error("invalid source path '" + path + "'");
  }

  std::istringstream stream(path);
  std::string segment;
  while(std::getline(stream, segment, '/')) {
    if(segment.empty() || segment == "." || segment == "..")
      throw reapack_error("invalid source path '" + path + "'");

    for(const unsigned char c : segment) {
      if(c < 0x20 || c == 0x7f)
        throw reapack_error("invalid source path '" + path + "'");
    }
  }

  if(path.back() == '/')
    throw reapack_error("invalid source path '" + path + "'");
}

Archive::SourceLocator Archive::makeSourceRootLocator(const Path &sourceRoot,
  const std::string &urlPrefix)
{
  if(!FS::exists(sourceRoot, true))
    throw reapack_error("source root not found: " + sourceRoot.join());
  if(!urlPrefix.empty() && urlPrefix.back() != '/')
    throw reapack_error("source URL prefix must end with '/'");

  const Path canonicalRoot = FS::canonical(sourceRoot);

  return [canonicalRoot, urlPrefix](const std::string &url) {
    std::string relative;

    if(urlPrefix.empty()) {
      if(url.find("://") != std::string::npos || url.rfind("//", 0) == 0)
        throw reapack_error("absolute source URL requires a URL prefix: " + url);
      relative = url;
    }
    else {
      if(url.rfind(urlPrefix, 0) != 0)
        throw reapack_error("source URL is outside the configured prefix: " + url);
      relative = url.substr(urlPrefix.size());
    }

    relative = decodeUrlPath(relative);
    validateSourcePath(relative);

    const Path source = FS::canonical(canonicalRoot + Path(relative));
    if(!source.startsWith(canonicalRoot))
      throw reapack_error("source path escapes the configured root: " + relative);

    return source;
  };
}

Archive::BuildPlan Archive::resolve(const RepositoryInput &input)
{
  if(!input.repository)
    throw reapack_error("invalid repository");
  if(input.index.empty())
    throw reapack_error("empty index path");
  if(!input.locateSource)
    throw reapack_error("source locator is not configured");

  std::ifstream indexStream;
  if(!FS::open(indexStream, input.index))
    throw reapack_error("index file not found: " + input.index.join());

  std::ostringstream indexContents;
  indexContents << indexStream.rdbuf();

  Index::LoadOptions loadOptions;
  loadOptions.rejectUnsupportedPackageTypes = true;
  loadOptions.rejectUnsupportedPlatforms = true;
  loadOptions.rejectUnsupportedSourceTypes = true;
  const IndexPtr index = Index::load(input.repository.name(),
    indexContents.str().c_str(), loadOptions);

  BuildPlan plan;
  plan.repository = input.repository;
  plan.index = input.index;

  for(const Package *package : index->packages()) {
    const Version *version = package->lastVersion();
    if(!version)
      continue;

    plan.packages.push_back({
      package->category()->name(),
      package->name(),
      version->name().toString(),
      0,
    });

    for(const Source *source : version->sources()) {
      plan.payloads.push_back({
        input.locateSource(source->url()),
        source->targetPath().join(false),
        source->checksum(),
      });
    }
  }

  return plan;
}
