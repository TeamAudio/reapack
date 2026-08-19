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

#include "transfer.hpp"

#include "hash.hpp"

#include <cassert>
#include <memory>
#include <mutex>

#include <curl/curl.h>

static constexpr long TRANSFER_TIMEOUT = 15;

static CURLSH *g_curlShare = nullptr;
static std::mutex g_curlMutex;

static void lockCurlMutex(CURL *, curl_lock_data, curl_lock_access, void *)
{
  g_curlMutex.lock();
}

static void unlockCurlMutex(CURL *, curl_lock_data, void *)
{
  g_curlMutex.unlock();
}

class CurlContext {
public:
  CurlContext();
  ~CurlContext();

  operator CURL*() { return m_curl; }

private:
  CURL *m_curl;
};

CurlContext::CurlContext() : m_curl(curl_easy_init())
{
  curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
  curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, TRANSFER_TIMEOUT);
  curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, TRANSFER_TIMEOUT);
  curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(m_curl, CURLOPT_SHARE, g_curlShare);
  curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
#ifdef CURLSSLOPT_REVOKE_BEST_EFFORT
  curl_easy_setopt(m_curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_REVOKE_BEST_EFFORT);
#endif
}

CurlContext::~CurlContext()
{
  curl_easy_cleanup(m_curl);
}

struct WriteContext {
  std::ostream *stream;
  std::unique_ptr<Hash> hash;

  void write(const char *data, size_t length)
  {
    stream->write(data, length);
    if(hash)
      hash->addData(data, length);
  }
};

static size_t writeData(char *data, size_t rawSize, size_t count, void *context)
{
  const size_t size = rawSize * count;
  static_cast<WriteContext *>(context)->write(data, size);
  return size;
}

static int updateProgress(void *context, double, double, double, double)
{
  const auto &aborted = *static_cast<const std::function<bool()> *>(context);
  return aborted && aborted();
}

void Transfer::globalInit()
{
  curl_global_init(CURL_GLOBAL_DEFAULT);

  g_curlShare = curl_share_init();
  assert(g_curlShare);

  curl_share_setopt(g_curlShare, CURLSHOPT_LOCKFUNC, lockCurlMutex);
  curl_share_setopt(g_curlShare, CURLSHOPT_UNLOCKFUNC, unlockCurlMutex);
  curl_share_setopt(g_curlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(g_curlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
}

void Transfer::globalCleanup()
{
  curl_share_cleanup(g_curlShare);
  curl_global_cleanup();
}

Transfer::Result Transfer::perform(const Request &request,
  std::ostream &stream)
{
  WriteContext write{&stream};

  Hash::Algorithm algorithm;
  if(!request.expectedChecksum.empty()) {
    if(!Hash::getAlgorithm(request.expectedChecksum, &algorithm)) {
      return {Failure::Checksum, 0,
        "Unsupported checksum: " + request.expectedChecksum};
    }
    write.hash = std::make_unique<Hash>(algorithm);
  }

  CurlContext context;
  curl_easy_setopt(context, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(context, CURLOPT_USERAGENT, request.userAgent.c_str());
  curl_easy_setopt(context, CURLOPT_PROXY, request.proxy.c_str());
  curl_easy_setopt(context, CURLOPT_SSL_VERIFYPEER,
    request.verifyPeer ? 1L : 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
  if(request.httpOnly) {
    curl_easy_setopt(context, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(context, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
  }
#else
  if(request.httpOnly) {
    curl_easy_setopt(context, CURLOPT_PROTOCOLS,
      CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(context, CURLOPT_REDIR_PROTOCOLS,
      CURLPROTO_HTTP | CURLPROTO_HTTPS);
  }
#endif
#ifdef __APPLE__
  curl_easy_setopt(context, CURLOPT_CAINFO, nullptr);
#endif

  std::function<bool()> aborted = request.aborted;
  curl_easy_setopt(context, CURLOPT_PROGRESSFUNCTION, updateProgress);
  curl_easy_setopt(context, CURLOPT_PROGRESSDATA, &aborted);
  curl_easy_setopt(context, CURLOPT_WRITEFUNCTION, writeData);
  curl_easy_setopt(context, CURLOPT_WRITEDATA, &write);

  curl_slist *headers = nullptr;
  for(const std::string &header : request.headers)
    headers = curl_slist_append(headers, header.c_str());
  curl_easy_setopt(context, CURLOPT_HTTPHEADER, headers);

  std::string errorBuffer = "No error message";
  errorBuffer.resize(CURL_ERROR_SIZE - 1, '\0');
  curl_easy_setopt(context, CURLOPT_ERRORBUFFER, errorBuffer.data());

  const CURLcode code = curl_easy_perform(context);
  curl_slist_free_all(headers);

  long status = 0;
  curl_easy_getinfo(context, CURLINFO_RESPONSE_CODE, &status);

  if(code != CURLE_OK) {
    return {Failure::Transport, status,
      std::string(curl_easy_strerror(code)) + " (" +
        std::to_string(static_cast<int>(code)) + "): " + errorBuffer.c_str()};
  }

  if(write.hash && write.hash->digest() != request.expectedChecksum) {
    return {Failure::Checksum, status,
      "Checksum mismatch.\nExpected: " + request.expectedChecksum +
        "\nActual: " + write.hash->digest()};
  }

  return {Failure::None, status, {}};
}
