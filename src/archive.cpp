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

#include "archive.hpp"

#include "config.hpp"
#include "errors.hpp"
#include "filesystem.hpp"
#include "index.hpp"
#include "path.hpp"
#include "reapack.hpp"
#include "transaction.hpp"

#include <iomanip>
#include <sstream>

static const Path ARCHIVE_TOC("toc");

struct ImportArchive {
  void importRemote(const std::string &);
  void importPackage(const std::string &);

  ArchiveReaderPtr m_reader;
  RemoteList *m_remotes;
  Transaction *m_tx;
  IndexPtr m_lastIndex;
};

void Archive::import(const std::string &path)
{
  ImportArchive state{std::make_shared<ArchiveReader>(path),
    &g_reapack->config()->remotes};

  std::stringstream toc;
  if(const int err = state.m_reader->extractFile(ARCHIVE_TOC, toc))
    throw reapack_error(String::format(
      "Cannot locate the table of contents (%d)", err));

  // starting import, do not abort process (eg. by throwing) at this point
  if(!(state.m_tx = g_reapack->setupTransaction()))
    return;

  std::string line;
  while(std::getline(toc, line)) {
    if(line.size() <= 5) // 5 is the length of the line type prefix
      continue;

    const std::string &data = line.substr(5);

    try {
      switch(line[0]) {
      case 'R':
        state.importRemote(data);
        break;
      case 'P':
        state.importPackage(data);
        break;
      default:
        throw reapack_error(String::format("Unknown token '%s' (skipping)",
          line.substr(0, 4).c_str()));
      }
    }
    catch(const reapack_error &e) {
      state.m_tx->receipt()->addError({e.what(), path});
    }
  }

  g_reapack->config()->write();
  state.m_tx->runTasks();
}

void ImportArchive::importRemote(const std::string &data)
{
  m_lastIndex = nullptr; // clear the previous repository
  Remote remote = Remote::fromString(data);

  if(const int err = m_reader->extractFile(Index::pathFor(remote.name()))) {
    throw reapack_error(String::format("Failed to extract index of %s (%d)",
      remote.name().c_str(), err));
  }

  const Remote &original = m_remotes->get(remote.name());
  if(original.isProtected()) {
    remote.setUrl(original.url());
    remote.protect();
  }

  m_remotes->add(remote);
  m_lastIndex = Index::load(remote.name());
}

void ImportArchive::importPackage(const std::string &data)
{
  // don't report an error if the index isn't loaded assuming we already
  // did when failing to import the repository above
  if(!m_lastIndex)
    return;

  std::string categoryName, packageName, versionName;
  int flags;

  std::istringstream stream(data);
  stream
    >> quoted(categoryName) >> quoted(packageName) >> quoted(versionName)
    >> flags;

  const Package *pkg = m_lastIndex->find(categoryName, packageName);
  const Version *ver = pkg ? pkg->findVersion(versionName) : nullptr;

  if(!ver) {
    throw reapack_error(String::format(
      "%s/%s/%s v%s cannot be found or is incompatible with your operating system.",
      m_lastIndex->name().c_str(), categoryName.c_str(),
      packageName.c_str(), versionName.c_str()));
  }

  m_tx->install(ver, flags, m_reader);
}
