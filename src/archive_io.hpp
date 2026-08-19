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

#ifndef REAPACK_ARCHIVE_IO_HPP
#define REAPACK_ARCHIVE_IO_HPP

#include "path.hpp"

#include <iosfwd>
#include <memory>

typedef void *zipFile;

class ArchiveReader {
public:
  ArchiveReader(const Path &path);
  ~ArchiveReader();
  int extractFile(const Path &);
  int extractFile(const Path &, std::ostream &) noexcept;

private:
  zipFile m_zip;
};

typedef std::shared_ptr<ArchiveReader> ArchiveReaderPtr;

class ArchiveWriter {
public:
  ArchiveWriter(const Path &path);
  ~ArchiveWriter();
  int addFile(const Path &fn);
  int addFile(const Path &fn, std::istream &) noexcept;

private:
  zipFile m_zip;
};

typedef std::shared_ptr<ArchiveWriter> ArchiveWriterPtr;

#endif
