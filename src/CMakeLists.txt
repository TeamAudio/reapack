include(CheckCXXCompilerFlag)

find_package(Boost 1.56 REQUIRED)
find_package(CURL       REQUIRED)
find_package(MiniZip    REQUIRED)

if(WIN32)
  find_package(tinyxml2 CONFIG REQUIRED)
else()
  find_package(LibXml2 REQUIRED)

  if(NOT APPLE)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
  endif()
endif()

add_library(reapack_archive STATIC
  archive_builder.cpp
  archive_io.cpp
  archive_manifest.cpp
  archive_resolver.cpp
  archive_source.cpp
  filesystem.cpp
  hash.cpp
  index.cpp
  index_v1.cpp
  metadata.cpp
  package.cpp
  path.cpp
  platform.cpp
  remote.cpp
  source.cpp
  string.cpp
  time.cpp
  transfer.cpp
  unicode.cpp
  version.cpp
  xml_$<IF:$<BOOL:${LIBXML2_FOUND}>,libxml2,tinyxml2>.cpp
)

target_compile_features(reapack_archive PUBLIC cxx_std_17)
target_include_directories(reapack_archive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(reapack_archive PUBLIC
  Boost::headers MiniZip::MiniZip
  $<IF:$<BOOL:${LIBXML2_FOUND}>,LibXml2::LibXml2,tinyxml2::tinyxml2>
)
target_link_libraries(reapack_archive PRIVATE CURL::libcurl)
target_compile_definitions(reapack_archive PRIVATE CURL_DISABLE_DEPRECATION)

if(WIN32)
  target_compile_options(reapack_archive PRIVATE /W3 /WX /wd4996)
  target_compile_definitions(reapack_archive PUBLIC NOMINMAX UNICODE)
  target_link_libraries(reapack_archive PUBLIC Bcrypt)
else()
  target_compile_options(reapack_archive PRIVATE
    -Wall -Wextra -Werror
    -Wno-unused-parameter -Wno-missing-field-initializers
  )
endif()

if(OPENSSL_FOUND)
  if(RUNTIME_OPENSSL)
    target_compile_definitions(reapack_archive PRIVATE RUNTIME_OPENSSL)
    target_link_libraries(reapack_archive PRIVATE ${CMAKE_DL_LIBS})
  else()
    target_link_libraries(reapack_archive PUBLIC OpenSSL::Crypto)
  endif()
endif()

set(REAPACK_ARCHIVE_VERSION "dev" CACHE STRING
  "Version string printed by reapack-archive --version")

add_executable(reapack-archive archive_cli.cpp)
target_compile_definitions(reapack-archive PRIVATE
  REAPACK_ARCHIVE_VERSION="${REAPACK_ARCHIVE_VERSION}")
target_link_libraries(reapack-archive PRIVATE reapack_archive)

if(NOT REAPACK_BUILD_PLUGIN)
  return()
endif()

find_package(Threads REQUIRED)
find_package(WDL     REQUIRED)

if(WIN32)
  # required for selecting the right debug or release version
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
  add_library(SQLite::SQLite3 INTERFACE IMPORTED)
  target_link_libraries(SQLite::SQLite3 INTERFACE unofficial::sqlite3::sqlite3)
else()
  find_package(SWELL REQUIRED)
  find_package(SQLite3 REQUIRED)
endif()

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    OUTPUT_VARIABLE REAPACK_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE IS_GIT_BUILD
    ERROR_QUIET
  )
endif()

if(NOT IS_GIT_BUILD EQUAL 0)
  set(REAPACK_REVISION "nogit")
endif()

get_target_property(REAPACK_BASENAME reaper_reapack OUTPUT_NAME)
set(REAPACK_FILENAME "${REAPACK_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")

file(STRINGS ../Extensions/ReaPack.ext REAPACK_VERSION REGEX "^@version")
string(SUBSTRING ${REAPACK_VERSION} 9 -1 REAPACK_VERSION)
string(REGEX REPLACE "[^0-9]+" ";" VERSION_LIST ${REAPACK_VERSION})
list(TRANSFORM VERSION_LIST REPLACE "[^0-9]" "")
list(APPEND VERSION_LIST 0 0 0)
list(GET VERSION_LIST 0 REAPACK_VERSION_MAJOR)
list(GET VERSION_LIST 1 REAPACK_VERSION_MINOR)
list(GET VERSION_LIST 2 REAPACK_VERSION_PATCH)
list(GET VERSION_LIST 3 REAPACK_VERSION_TWEAK)

string(TIMESTAMP REAPACK_BUILDTIME "%b %d %Y %H:%M:%S" UTC)
file(STRINGS ../ABOUT.md REAPACK_COPYRIGHT REGEX "^Copyright")

configure_file(buildinfo.hpp.in buildinfo.hpp)

add_library(reapack OBJECT
  about.cpp
  action.cpp
  api.cpp
  api_misc.cpp
  api_package.cpp
  api_repo.cpp
  archive.cpp
  archive_tasks.cpp
  browser.cpp
  browser_entry.cpp
  config.cpp
  control.cpp
  database.cpp
  dialog.cpp $<IF:$<BOOL:${APPLE}>,dialog.mm,>
  download.cpp
  event.cpp
  filedialog.cpp
  filter.cpp
  iconlist.cpp
  import.cpp
  install.cpp
  listview.cpp
  main.cpp
  manager.cpp
  menu.cpp
  obsquery.cpp
  progress.cpp
  reapack.cpp
  receipt.cpp
  registry.cpp
  report.cpp
  resource.rc
  richedit$<IF:$<BOOL:${APPLE}>,.mm,$<IF:$<BOOL:${WIN32}>,-win32,-generic>.cpp>
  serializer.cpp
  synchronize.cpp
  tabbar.cpp
  task.cpp
  thread.cpp
  transaction.cpp
  win32.cpp
)

target_compile_features(reapack PUBLIC cxx_std_17)
target_include_directories(reapack PRIVATE
  ${CMAKE_SOURCE_DIR}/vendor ${CMAKE_SOURCE_DIR}/vendor/reaper-sdk/sdk
  ${CMAKE_CURRENT_BINARY_DIR}
)

if(WIN32)
  target_sources(reapack PRIVATE buildinfo.rc)
  target_compile_options(reapack PUBLIC /W3 /WX /wd4996)
  target_compile_definitions(reapack PUBLIC NOMINMAX UNICODE)
  target_link_libraries(reapack Bcrypt Comctl32 Comdlg32 Gdi32 Shell32 User32)
else()
  target_compile_options(reapack PUBLIC
    -Wall -Wextra -Werror
    -Wno-unused-parameter -Wno-missing-field-initializers
  )
endif()

target_link_libraries(reapack
  reapack_archive
  ${CMAKE_DL_LIBS} Boost::headers MiniZip::MiniZip
  SQLite::SQLite3 Threads::Threads WDL::WDL
  $<IF:$<BOOL:${LIBXML2_FOUND}>,LibXml2::LibXml2,tinyxml2::tinyxml2>
)

if(SWELL_FOUND)
  target_link_libraries(reapack SWELL::swell)

  # Transpile the Win32 resource file
  find_package(PHP REQUIRED)
  add_custom_command(
    OUTPUT  resource.rc_mac_dlg resource.rc_mac_menu
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/resource.rc ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${PHP_EXECUTABLE} ${SWELL_RESGEN} --quiet resource.rc
    DEPENDS ${SWELL_RESGEN}
    MAIN_DEPENDENCY resource.rc
  )
endif()
