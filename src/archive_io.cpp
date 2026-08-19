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

#include "archive_io.hpp"

#include "errors.hpp"
#include "filesystem.hpp"
#include "unicode.hpp"

#include <fstream>

#include <ioapi.h>
#include <unzip.h>
#include <zip.h>

static const size_t BUFFER_SIZE = 4096;

#ifdef _WIN32
static void *wide_fopen(voidpf, const void *filename, int mode)
{
  const wchar_t *fopen_mode = nullptr;

  if((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ)
    fopen_mode = L"rb";
  else if(mode & ZLIB_FILEFUNC_MODE_EXISTING)
    fopen_mode = L"r+b";
  else if(mode & ZLIB_FILEFUNC_MODE_CREATE)
    fopen_mode = L"wb";

  FILE *file = nullptr;

  if(filename && fopen_mode) {
    const auto &&wideFilename = Unicode::widen(static_cast<const char *>(filename));
    _wfopen_s(&file, wideFilename.c_str(), fopen_mode);
  }

  return file;
}
#endif

ArchiveReader::ArchiveReader(const Path &path)
{
  zlib_filefunc64_def filefunc;
  fill_fopen64_filefunc(&filefunc);
#ifdef _WIN32
  filefunc.zopen64_file = wide_fopen;
#endif

  m_zip = unzOpen2_64(path.join().c_str(), &filefunc);

  if(!m_zip)
    throw reapack_error(FS::lastError());
}

ArchiveReader::~ArchiveReader()
{
  unzClose(m_zip);
}

int ArchiveReader::extractFile(const Path &path)
{
  std::ofstream stream;

  if(FS::open(stream, path))
    return extractFile(path, stream);
  else {
    throw reapack_error(String::format("%s: %s",
      path.join().c_str(), FS::lastError()));
  }
}

int ArchiveReader::extractFile(const Path &path, std::ostream &stream) noexcept
{
  int status = unzLocateFile(m_zip, path.join(false).c_str(), false);
  if(status != UNZ_OK)
    return status;

  status = unzOpenCurrentFile(m_zip);
  if(status != UNZ_OK)
    return status;

  std::string buffer(BUFFER_SIZE, 0);

  const auto readChunk = [&] {
    return unzReadCurrentFile(m_zip, &buffer[0], static_cast<int>(buffer.size()));
  };

  while(const int len = readChunk()) {
    if(len < 0)
      return len; // read error

    stream.write(&buffer[0], len);
  }

  return unzCloseCurrentFile(m_zip);
}

ArchiveWriter::ArchiveWriter(const Path &path)
{
  zlib_filefunc64_def filefunc;
  fill_fopen64_filefunc(&filefunc);
#ifdef _WIN32
  filefunc.zopen64_file = wide_fopen;
#endif

  m_zip = zipOpen2_64(path.join().c_str(), APPEND_STATUS_CREATE, nullptr, &filefunc);

  if(!m_zip)
    throw reapack_error(FS::lastError());
}

ArchiveWriter::~ArchiveWriter()
{
  zipClose(m_zip, nullptr);
}

int ArchiveWriter::addFile(const Path &path)
{
  std::ifstream stream;

  if(FS::open(stream, path))
    return addFile(path, stream);
  else {
    throw reapack_error(String::format("%s: %s",
      path.join().c_str(), FS::lastError()));
  }
}

int ArchiveWriter::addFile(const Path &path, std::istream &stream) noexcept
{
  const int status = zipOpenNewFileInZip(m_zip, path.join(false).c_str(), nullptr,
    nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);

  if(status != ZIP_OK)
    return status;

  std::string buffer(BUFFER_SIZE, 0);

  const auto readChunk = [&] {
    stream.read(&buffer[0], buffer.size());
    return static_cast<int>(stream.gcount());
  };

  while(const int len = readChunk()) {
    if(len < 0)
      return len; // write error

    zipWriteInFileInZip(m_zip, &buffer[0], len);
  }

  return zipCloseFileInZip(m_zip);
}
