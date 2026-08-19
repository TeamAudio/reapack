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

#include "task.hpp"

#include "archive.hpp"
#include "archive_manifest.hpp"
#include "config.hpp"
#include "filesystem.hpp"
#include "index.hpp"
#include "reapack.hpp"
#include "remote.hpp"
#include "string.hpp"
#include "transaction.hpp"

#include <fstream>
#include <sstream>

static const Path ARCHIVE_TOC("toc");

FileExtractor::FileExtractor(const Path &target, const ArchiveReaderPtr &reader)
  : m_path(target), m_reader(reader)
{
  setSummary({ "Extracting", target.join() });
}

bool FileExtractor::run()
{
  std::ofstream stream;
  if(!FS::open(stream, m_path.temp())) {
    setError({FS::lastError(), m_path.temp().join()});
    return false;
  }

  const int error = m_reader->extractFile(m_path.target(), stream);
  stream.close();

  if(error) {
    setError({String::format("Failed to extract file (%d)", error),
      m_path.target().join()});
    return false;
  }

  return true;
}

FileCompressor::FileCompressor(const Path &target, const ArchiveWriterPtr &writer)
  : m_path(target), m_writer(writer)
{
  setSummary({ "Compressing", target.join() });
}

bool FileCompressor::run()
{
  std::ifstream stream;
  if(!FS::open(stream, m_path)) {
    setError({
      String::format("Could not open file for export (%s)", FS::lastError()),
      m_path.join()});
    return false;
  }

  const int error = m_writer->addFile(m_path, stream);
  stream.close();

  if(error) {
    setError({String::format("Failed to compress file (%d)", error), m_path.join()});
    return false;
  }

  return true;
}

ExportTask::ExportTask(const std::string &path, Transaction *tx)
  : Task(tx), m_path(path)
{
}

bool ExportTask::start()
{
  std::stringstream toc;
  ArchiveWriterPtr writer;

  try {
    writer = std::make_shared<ArchiveWriter>(m_path.temp());
  }
  catch(const reapack_error &e) {
    tx()->receipt()->addError({
      String::format("Could not open archive for writing: %s", e.what()),
      m_path.temp().join()
    });
    return false;
  }

  std::vector<FileCompressor *> jobs;

  for(const Remote &remote : g_reapack->config()->remotes.getEnabled()) {
    bool addedRemote = false;

    for(const Registry::Entry &entry : tx()->registry()->getEntries(remote.name())) {
      if(!addedRemote) {
        Archive::writeRepositoryRecord(toc, remote);
        jobs.push_back(new FileCompressor(Index::pathFor(remote.name()), writer));
        addedRemote = true;
      }

      const Archive::PackageRecord package{
        entry.category,
        entry.package,
        entry.version.toString(),
        entry.flags,
      };
      Archive::writePackageRecord(toc, package);

      for(const Registry::File &file : tx()->registry()->getFiles(entry))
        jobs.push_back(new FileCompressor(file.path, writer));
    }
  }

  writer->addFile(ARCHIVE_TOC, toc);

  // Start after we've written the table of contents in the main thread
  // because we cannot safely write into the zip from more than one
  // thread at the same time.
  for(FileCompressor *job : jobs) {
    job->onFinishAsync >> [=] {
      if(job->state() == ThreadTask::Success)
        tx()->receipt()->addExport(job->path());
    };

    tx()->threadPool()->push(job);
  }

  return true;
}

void ExportTask::commit()
{
  if(!FS::rename(m_path)) {
    tx()->receipt()->addError({
      String::format("Could not move to permanent location: %s", FS::lastError()),
      m_path.target().prependRoot().join()
    });
  }
}

void ExportTask::rollback()
{
  FS::remove(m_path.temp());
}
