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

#ifndef REAPACK_ARCHIVE_SOURCE_HPP
#define REAPACK_ARCHIVE_SOURCE_HPP

#include "archive_resolver.hpp"

#include <map>
#include <string>
#include <vector>

namespace Archive {
  struct SourceAcquireOptions {
    Path sourceRoot;
    std::string sourceUrlPrefix;
    Path stagingRoot;
    bool offline{};
  };

  class SourceAcquirer {
  public:
    explicit SourceAcquirer(const SourceAcquireOptions &);
    SourceAcquirer(const SourceAcquirer &) = delete;
    ~SourceAcquirer();

    SourceLocator locator();

  private:
    Path locate(const std::string &url, const std::string &checksum);
    Path download(const std::string &url, const std::string &checksum);

    SourceAcquireOptions m_options;
    SourceLocator m_relativeLocator;
    SourceLocator m_prefixedLocator;
    std::map<std::string, Path> m_downloads;
    std::vector<Path> m_stagedFiles;
  };
}

#endif
