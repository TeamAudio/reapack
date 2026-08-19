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

#ifndef REAPACK_ARCHIVE_RESOLVER_HPP
#define REAPACK_ARCHIVE_RESOLVER_HPP

#include "archive_builder.hpp"

#include <functional>

namespace Archive {
  using SourceLocator = std::function<Path(const std::string &url)>;

  struct RepositoryInput {
    Remote repository;
    Path index;
    SourceLocator locateSource;
  };

  SourceLocator makeSourceRootLocator(const Path &sourceRoot,
    const std::string &urlPrefix = {});
  BuildPlan resolve(const RepositoryInput &);
};

#endif
