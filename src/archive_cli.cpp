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
#include "archive_source.hpp"
#include "remote.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

static constexpr const char *DEFAULT_REPOSITORY_URL =
  "https://reapack-preview.invalid/index.xml";

struct Options {
  std::string index;
  std::string sourceRoot;
  std::string sourceUrlPrefix;
  std::string repositoryName;
  std::string repositoryUrl{DEFAULT_REPOSITORY_URL};
  std::string output;
  bool offline{};
  bool help{};
};

class usage_error : public std::runtime_error {
public:
  using runtime_error::runtime_error;
};

static void printUsage(std::ostream &stream)
{
  stream <<
    "Usage: reapack-archive [options]\n"
    "\n"
    "Build an offline ReaPack archive from a repository index.\n"
    "The latest package versions are resolved for the current platform.\n"
    "The generated repository is disabled to prevent automatic synchronization.\n"
    "Relative and prefix-mapped sources are read locally; other HTTP(S)\n"
    "sources are downloaded unless --offline is used.\n"
    "\n"
    "Required options:\n"
    "  --index PATH                Local index.xml\n"
    "  --source-root PATH          Root containing indexed source files\n"
    "  --repository-name NAME      Repository identity shown in ReaPack\n"
    "  --output PATH               Output .ReaPackArchive\n"
    "\n"
    "Optional:\n"
    "  --source-url-prefix URL     Prefix stripped from absolute source URLs\n"
    "  --offline                   Reject sources that require downloading\n"
    "  --repository-url URL        Stored URL (default: offline placeholder)\n"
    "  -h, --help                  Show this help\n";
}

static std::string takeValue(const int argc, char **argv, int *index,
  const std::string &option)
{
  if(*index + 1 >= argc)
    throw usage_error("missing value for " + option);

  return argv[++*index];
}

static void setOption(std::string *target, const std::string &value,
  const std::string &option)
{
  if(!target->empty())
    throw usage_error("duplicate option " + option);
  if(value.empty())
    throw usage_error("empty value for " + option);

  *target = value;
}

static Options parseOptions(const int argc, char **argv)
{
  Options options;
  bool repositoryUrlSet = false;

  for(int i = 1; i < argc; ++i) {
    const std::string option(argv[i]);

    if(option == "-h" || option == "--help")
      options.help = true;
    else if(option == "--index")
      setOption(&options.index, takeValue(argc, argv, &i, option), option);
    else if(option == "--source-root")
      setOption(&options.sourceRoot, takeValue(argc, argv, &i, option), option);
    else if(option == "--source-url-prefix")
      setOption(&options.sourceUrlPrefix, takeValue(argc, argv, &i, option), option);
    else if(option == "--offline")
      options.offline = true;
    else if(option == "--repository-name")
      setOption(&options.repositoryName, takeValue(argc, argv, &i, option), option);
    else if(option == "--repository-url") {
      if(repositoryUrlSet)
        throw usage_error("duplicate option " + option);
      options.repositoryUrl = takeValue(argc, argv, &i, option);
      repositoryUrlSet = true;
    }
    else if(option == "--output")
      setOption(&options.output, takeValue(argc, argv, &i, option), option);
    else
      throw usage_error("unknown option " + option);
  }

  if(options.help)
    return options;

  if(options.index.empty())
    throw usage_error("missing required option --index");
  if(options.sourceRoot.empty())
    throw usage_error("missing required option --source-root");
  if(options.repositoryName.empty())
    throw usage_error("missing required option --repository-name");
  if(options.output.empty())
    throw usage_error("missing required option --output");

  return options;
}

int main(const int argc, char **argv)
{
  try {
    const Options options = parseOptions(argc, argv);
    if(options.help) {
      printUsage(std::cout);
      return 0;
    }

    Archive::RepositoryInput input;
    input.repository = Remote(options.repositoryName, options.repositoryUrl,
      false, false);
    input.index = Path(options.index);

    Archive::SourceAcquireOptions sourceOptions;
    sourceOptions.sourceRoot = Path(options.sourceRoot);
    sourceOptions.sourceUrlPrefix = options.sourceUrlPrefix;
    sourceOptions.stagingRoot = Path(options.output + ".sources.part");
    sourceOptions.offline = options.offline;
    Archive::SourceAcquirer sources(sourceOptions);
    input.locateSource = sources.locator();

    const Archive::BuildPlan plan = Archive::resolve(input);
    Archive::build(plan, Path(options.output));

    std::cout << "Created " << options.output << " with "
      << plan.packages.size() << " packages and "
      << plan.payloads.size() << " files.\n";
    return 0;
  }
  catch(const usage_error &error) {
    std::cerr << "reapack-archive: " << error.what() << "\n\n";
    printUsage(std::cerr);
    return 2;
  }
  catch(const std::exception &error) {
    std::cerr << "reapack-archive: " << error.what() << '\n';
    return 1;
  }
}
