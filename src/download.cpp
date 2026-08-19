/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2025  Christian Fillion
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

#include "download.hpp"

#include "buildinfo.hpp"
#include "filesystem.hpp"
#include "reapack.hpp"
#include "transfer.hpp"
#include "win32.hpp"

#include <reaper_plugin_functions.h>

Download::Download(const std::string &url, const NetworkOpts &opts, const int flags)
  : m_url(url), m_opts(opts), m_flags(flags)
{
  onRequestProxyAsync >> std::bind(&ReaPack::requestProxy, g_reapack);
}

void Download::setName(const std::string &name)
{
  setSummary({ "Downloading", name });
}

static bool isGitHub(const std::string &url)
{
  constexpr std::string_view patterns[] {
    "https://github.com/", "https://www.github.com/",
    "https://raw.githubusercontent.com/",
  };
  for(const std::string_view &pattern : patterns) {
    if(url.rfind(pattern, 0) == 0)
      return true;
  }
  return false;
}

bool Download::run(const bool proxy)
{
  std::ostream *stream = openStream();
  if(!stream)
    return false;

  Transfer::Request request;
  request.url = m_url;
  request.proxy = m_opts.proxy;
  request.verifyPeer = m_opts.verifyPeer;
  request.expectedChecksum = m_expectedChecksum;
  request.aborted = [this] { return aborted(); };

  char userAgent[64];
  snprintf(userAgent, sizeof(userAgent), "ReaPack/%s REAPER/%s",
    REAPACK_VERSION, GetAppVersion());
  request.userAgent = userAgent;

  if(has(Download::NoCacheFlag))
    request.headers.push_back("Cache-Control: no-cache");
  if(proxy) {
    request.url.insert(0, "https://raw.reapack.com/usercontent?");
    request.headers.push_back("X-ReaPack-Proxy: 1");
  }

  Transfer::Result result = Transfer::perform(request, *stream);
  closeStream();

  if(!result) {
    if(result.failure == Transfer::Failure::Transport
        && !proxy && isGitHub(m_url) && result.status == 429
        && *(onRequestProxyAsync().get()))
      return run(true);

#ifdef _WIN32
    if(result.failure == Transfer::Failure::Transport)
      result.error = Win32::ansi2utf8(result.error);
#endif

    if(proxy && result.failure == Transfer::Failure::Transport)
      result.error += " [proxied]";
    setError({result.error, m_url});
    return false;
  }

  return true;
}

MemoryDownload::MemoryDownload(const std::string &url, const NetworkOpts &opts, int flags)
  : Download(url, opts, flags)
{
  setName(url);
}

FileDownload::FileDownload(const Path &target, const std::string &url,
    const NetworkOpts &opts, int flags)
  : Download(url, opts, flags), m_path(target)
{
  setName(target.join());
}

bool FileDownload::save()
{
  if(state() == Success)
    return FS::rename(m_path);
  else
    return FS::remove(m_path.temp());
}

std::ostream *FileDownload::openStream()
{
  if(FS::open(m_stream, m_path.temp()))
    return &m_stream;
  else {
    setError({FS::lastError(), m_path.temp().join()});
    return nullptr;
  }
}

void FileDownload::closeStream()
{
  m_stream.close();
}
