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

#ifndef REAPACK_TRANSFER_HPP
#define REAPACK_TRANSFER_HPP

#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace Transfer {
  enum class Failure {
    None,
    Transport,
    Checksum,
  };

  struct Request {
    std::string url;
    std::string userAgent;
    std::string proxy;
    std::string expectedChecksum;
    std::vector<std::string> headers;
    std::function<bool()> aborted;
    bool verifyPeer{true};
  };

  struct Result {
    Failure failure{};
    long status{};
    std::string error;

    explicit operator bool() const { return failure == Failure::None; }
  };

  void globalInit();
  void globalCleanup();
  Result perform(const Request &, std::ostream &);
}

#endif
