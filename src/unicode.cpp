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

#include "unicode.hpp"

#ifdef _WIN32
#include <windows.h>

std::wstring Unicode::widen(const char *input, const unsigned int codepage)
{
  const int size = MultiByteToWideChar(codepage, 0, input, -1, nullptr, 0);
  if(size <= 0)
    return {};

  std::wstring output(size, 0);
  MultiByteToWideChar(codepage, 0, input, -1, &output[0], size);
  output.pop_back();
  return output;
}

std::string Unicode::narrow(const wchar_t *input)
{
  const int size = WideCharToMultiByte(CP_UTF8, 0,
    input, -1, nullptr, 0, nullptr, nullptr);
  if(size <= 0)
    return {};

  std::string output(size, 0);
  WideCharToMultiByte(CP_UTF8, 0,
    input, -1, &output[0], size, nullptr, nullptr);
  output.pop_back();
  return output;
}
#endif
