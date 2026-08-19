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

#include "archive_builder.hpp"

#include "archive_io.hpp"
#include "errors.hpp"
#include "filesystem.hpp"
#include "hash.hpp"
#include "string.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

static const Path ARCHIVE_TOC("toc");

static Path indexPathFor(const Remote &repository)
{
  return Path::CACHE + (repository.name() + ".xml");
}

static std::string targetKey(std::string target)
{
  std::transform(target.begin(), target.end(), target.begin(),
    [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return target;
}

static void validateTarget(const std::string &target)
{
  if(target.empty() || target.front() == '/' || target.find('\\') != std::string::npos
      || target.find(':') != std::string::npos) {
    throw reapack_error("invalid archive target '" + target + "'");
  }

  std::istringstream stream(target);
  std::string segment;
  while(std::getline(stream, segment, '/')) {
    if(segment.empty() || segment == "." || segment == "..")
      throw reapack_error("invalid archive target '" + target + "'");
  }

  if(target.back() == '/')
    throw reapack_error("invalid archive target '" + target + "'");
}

static void validateField(const char *name, const std::string &value)
{
  if(value.empty())
    throw reapack_error(std::string("empty package ") + name);
  if(value.find_first_of("\r\n") != std::string::npos)
    throw reapack_error(std::string("invalid package ") + name);
}

static void addStream(ArchiveWriter &writer, const Path &target,
  std::istream &stream)
{
  if(const int error = writer.addFile(target, stream)) {
    throw reapack_error(String::format("Failed to add %s to archive (%d)",
      target.join(false).c_str(), error));
  }
}

static void addFile(ArchiveWriter &writer, const Path &target,
  const Path &source)
{
  std::ifstream stream;
  if(!FS::open(stream, source)) {
    throw reapack_error(String::format("Could not open %s: %s",
      source.join().c_str(), FS::lastError()));
  }

  addStream(writer, target, stream);
}

static void validateChecksum(const Archive::Payload &payload)
{
  if(payload.checksum.empty())
    return;

  Hash::Algorithm algorithm;
  if(!Hash::getAlgorithm(payload.checksum, &algorithm))
    throw reapack_error("invalid checksum for " + payload.source.join());

  std::ifstream stream;
  if(!FS::open(stream, payload.source)) {
    throw reapack_error(String::format("Could not open %s: %s",
      payload.source.join().c_str(), FS::lastError()));
  }

  Hash hash(algorithm);
  std::string buffer(4096, 0);
  while(stream.read(&buffer[0], buffer.size()) || stream.gcount())
    hash.addData(&buffer[0], static_cast<size_t>(stream.gcount()));

  if(hash.digest() != payload.checksum)
    throw reapack_error("checksum mismatch for " + payload.source.join());
}

void Archive::build(const BuildPlan &plan, const Path &output)
{
  if(!plan.repository)
    throw reapack_error("invalid repository");
  if(plan.index.empty())
    throw reapack_error("empty index path");
  if(output.empty())
    throw reapack_error("empty output path");
  if(!FS::exists(plan.index))
    throw reapack_error("index file not found: " + plan.index.join());

  const Path indexTarget = indexPathFor(plan.repository);
  std::set<std::string> targets{
    targetKey(ARCHIVE_TOC.join(false)),
    targetKey(indexTarget.join(false)),
  };

  for(const PackageSelection &package : plan.packages) {
    validateField("category", package.category);
    validateField("name", package.package);
    validateField("version", package.version);
  }

  for(const Payload &payload : plan.payloads) {
    validateTarget(payload.target);

    if(!targets.insert(targetKey(payload.target)).second)
      throw reapack_error("duplicate archive target '" + payload.target + "'");
    if(payload.source.empty() || !FS::exists(payload.source))
      throw reapack_error("payload file not found: " + payload.source.join());
    validateChecksum(payload);
  }

  std::ostringstream toc;
  toc << "REPO " << plan.repository.toString() << '\n';

  for(const PackageSelection &package : plan.packages) {
    toc << "PACK "
      << std::quoted(package.category) << ' '
      << std::quoted(package.package) << ' '
      << std::quoted(package.version) << ' '
      << package.flags << '\n';
  }

  const TempPath temporary(output);
  FS::remove(temporary.temp());

  try {
    {
      ArchiveWriter writer(temporary.temp());
      std::istringstream tocStream(toc.str());
      addStream(writer, ARCHIVE_TOC, tocStream);
      addFile(writer, indexTarget, plan.index);

      for(const Payload &payload : plan.payloads)
        addFile(writer, Path(payload.target), payload.source);
    }

    if(!FS::rename(temporary)) {
      throw reapack_error(String::format("Could not move archive into place: %s",
        FS::lastError()));
    }
  }
  catch(...) {
    FS::remove(temporary.temp());
    throw;
  }
}
