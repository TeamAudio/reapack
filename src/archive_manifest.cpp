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

#include "archive_manifest.hpp"

#include "remote.hpp"

#include <iomanip>
#include <ostream>

void Archive::writeRepositoryRecord(std::ostream &stream,
  const Remote &repository)
{
  stream << "REPO " << repository.toString() << '\n';
}

void Archive::writePackageRecord(std::ostream &stream,
  const PackageRecord &package)
{
  stream << "PACK "
    << std::quoted(package.category) << ' '
    << std::quoted(package.package) << ' '
    << std::quoted(package.version) << ' '
    << package.flags << '\n';
}
