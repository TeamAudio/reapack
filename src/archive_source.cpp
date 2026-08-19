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

#include "archive_source.hpp"

#include "errors.hpp"
#include "filesystem.hpp"
#include "transfer.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

static std::string getScheme(const std::string &url)
{
  const size_t separator = url.find("://");
  if(separator == std::string::npos)
    return {};

  std::string scheme = url.substr(0, separator);
  std::transform(scheme.begin(), scheme.end(), scheme.begin(),
    [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return scheme;
}

Archive::SourceAcquirer::SourceAcquirer(const SourceAcquireOptions &options)
  : m_options(options),
    m_relativeLocator(makeSourceRootLocator(options.sourceRoot))
{
  if(m_options.stagingRoot.empty())
    throw reapack_error("empty source staging path");
  if(FS::exists(m_options.stagingRoot)
      || FS::exists(m_options.stagingRoot, true)) {
    throw reapack_error("source staging path already exists: " +
      m_options.stagingRoot.join());
  }

  if(!m_options.sourceUrlPrefix.empty()) {
    m_prefixedLocator = makeSourceRootLocator(m_options.sourceRoot,
      m_options.sourceUrlPrefix);
  }

  Transfer::globalInit();
}

Archive::SourceAcquirer::~SourceAcquirer()
{
  for(const Path &path : m_stagedFiles)
    FS::remove(path);
  FS::remove(m_options.stagingRoot);

  Transfer::globalCleanup();
}

Archive::SourceLocator Archive::SourceAcquirer::locator()
{
  return [this](const std::string &url, const std::string &checksum) {
    return locate(url, checksum);
  };
}

Path Archive::SourceAcquirer::locate(const std::string &url,
  const std::string &checksum)
{
  if(m_prefixedLocator
      && url.rfind(m_options.sourceUrlPrefix, 0) == 0) {
    return m_prefixedLocator(url, checksum);
  }

  const std::string scheme = getScheme(url);
  if(scheme.empty()) {
    if(url.rfind("//", 0) == 0)
      throw reapack_error("unsupported source URL: " + url);
    return m_relativeLocator(url, checksum);
  }

  if(scheme != "http" && scheme != "https")
    throw reapack_error("unsupported source URL scheme '" + scheme + "'");
  if(m_options.offline)
    throw reapack_error("remote source unavailable in offline mode: " + url);

  return download(url, checksum);
}

Path Archive::SourceAcquirer::download(const std::string &url,
  const std::string &checksum)
{
  const auto existing = m_downloads.find(url);
  if(existing != m_downloads.end())
    return existing->second;

  const Path target = m_options.stagingRoot +
    ("source-" + std::to_string(m_downloads.size()));
  std::ofstream stream;
  if(!FS::open(stream, target)) {
    throw reapack_error("could not create staged source: " + target.join());
  }

  Transfer::Request request;
  request.url = url;
  request.userAgent = "ReaPack archive builder";
  request.expectedChecksum = checksum;
  request.httpOnly = true;

  Transfer::Result result;
  try {
    result = Transfer::perform(request, stream);
  }
  catch(...) {
    stream.close();
    FS::remove(target);
    throw;
  }
  stream.close();

  if(!result) {
    FS::remove(target);
    throw reapack_error("failed to download " + url + ": " + result.error);
  }

  m_downloads.emplace(url, target);
  m_stagedFiles.push_back(target);
  return target;
}
