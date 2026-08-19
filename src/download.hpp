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

#ifndef REAPACK_DOWNLOAD_HPP
#define REAPACK_DOWNLOAD_HPP

#include "config.hpp"
#include "path.hpp"
#include "thread.hpp"

#include <fstream>
#include <sstream>

class Download : public ThreadTask {
public:
  enum Flag {
    NoCacheFlag = 1<<0,
  };

  Download(const std::string &url, const NetworkOpts &, int flags = 0);

  void setName(const std::string &);
  void setExpectedChecksum(const std::string &checksum) {
    m_expectedChecksum = checksum;
  }
  const std::string &url() const { return m_url; }

  bool concurrent() const override { return true; }
  bool run() override { return run(false); }
  bool run(bool proxy);

  AsyncEvent<bool()> onRequestProxyAsync;

protected:
  virtual std::ostream *openStream() = 0;
  virtual void closeStream() {}

private:
  bool has(Flag f) const { return (m_flags & f) != 0; }

  std::string m_url;
  std::string m_expectedChecksum;
  NetworkOpts m_opts;
  int m_flags;
};

class MemoryDownload : public Download {
public:
  MemoryDownload(const std::string &url, const NetworkOpts &, int flags = 0);

  std::string contents() const { return m_stream.str(); }

protected:
  std::ostream *openStream() override { return &m_stream; }

private:
  std::stringstream m_stream;
};

class FileDownload : public Download {
public:
  FileDownload(const Path &target, const std::string &url,
    const NetworkOpts &, int flags = 0);

  const TempPath &path() const { return m_path; }
  bool save();

protected:
  std::ostream *openStream() override;
  void closeStream() override;

private:
  TempPath m_path;
  std::ofstream m_stream;
};

#endif
