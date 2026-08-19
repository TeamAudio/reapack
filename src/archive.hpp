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

#ifndef REAPACK_ARCHIVE_HPP
#define REAPACK_ARCHIVE_HPP

#include "archive_io.hpp"
#include "thread.hpp"

class ThreadPool;

namespace Archive {
  void import(const std::string &path);
};

class FileExtractor : public ThreadTask {
public:
  FileExtractor(const Path &target, const ArchiveReaderPtr &);
  const TempPath &path() const { return m_path; }

  bool concurrent() const override { return false; }
  bool run() override;

private:
  TempPath m_path;
  ArchiveReaderPtr m_reader;
};

class FileCompressor : public ThreadTask {
public:
  FileCompressor(const Path &target, const ArchiveWriterPtr &);
  const Path &path() const { return m_path; }

  bool concurrent() const override { return false; }
  bool run() override;

private:
  Path m_path;
  ArchiveWriterPtr m_writer;
};

#endif
