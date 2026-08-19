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

#ifndef REAPACK_UNICODE_HPP
#define REAPACK_UNICODE_HPP

#include <string>

namespace Unicode {
#ifdef _WIN32
  std::wstring widen(const char *, unsigned int codepage = 65001);
  inline std::wstring widen(const std::string &str,
    const unsigned int codepage = 65001)
  { return widen(str.c_str(), codepage); }

  std::string narrow(const wchar_t *);
  inline std::string narrow(const std::wstring &str) { return narrow(str.c_str()); }
#else
  inline std::string widen(const char *str, unsigned int = 0) { return str; }
  inline std::string widen(const std::string &str, unsigned int = 0) { return str; }

  inline std::string narrow(const char *str) { return str; }
  inline std::string narrow(const std::string &str) { return str; }
#endif
};

#endif
