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

#include "index.hpp"

#include "errors.hpp"
#include "xml.hpp"

#include <sstream>

static void LoadMetadataV1(XmlNode, Metadata *);
static void LoadCategoryV1(XmlNode, Index *, const Index::LoadOptions &);
static void LoadPackageV1(XmlNode, Category *, const Index::LoadOptions &);
static void LoadVersionV1(XmlNode, Package *, const Index::LoadOptions &);
static void LoadSourceV1(XmlNode, Version *, bool hasArm64Ec,
  const Index::LoadOptions &);

void Index::loadV1(XmlNode root, Index *ri, const LoadOptions &options)
{
  if(ri->name().empty()) {
    if(const XmlString &name = root.attribute("name"))
      ri->setName(*name);
  }

  for(XmlNode node = root.firstChild("category");
      node; node = node.nextSibling("category"))
    LoadCategoryV1(node, ri, options);

  if(XmlNode node = root.firstChild("metadata"))
    LoadMetadataV1(node, ri->metadata());
}

void LoadMetadataV1(XmlNode meta, Metadata *md)
{
  XmlNode node = meta.firstChild("description");

  if(node) {
    if(const XmlString &rtf = node.text())
      md->setAbout(*rtf);
  }

  node = meta.firstChild("link");

  for(node = meta.firstChild("link"); node; node = node.nextSibling("link")) {
    const XmlString &rel  = node.attribute("rel"),
                    &url  = node.attribute("href"),
                    &name = node.text();

    std::string effectiveName{name ? *name : url.value_or("")};

    md->addLink(Metadata::getLinkType(rel.value_or("")),
      {effectiveName, url.value_or(effectiveName.c_str())});
  }
}

void LoadCategoryV1(XmlNode catNode, Index *ri,
  const Index::LoadOptions &options)
{
  const XmlString &name = catNode.attribute("name");

  Category *cat = new Category(name.value_or(""), ri);
  std::unique_ptr<Category> ptr(cat);

  for(XmlNode packNode = catNode.firstChild("reapack");
      packNode; packNode = packNode.nextSibling("reapack"))
    LoadPackageV1(packNode, cat, options);

  if(ri->addCategory(cat))
    ptr.release();
}

void LoadPackageV1(XmlNode packNode, Category *cat,
  const Index::LoadOptions &options)
{
  const XmlString &type = packNode.attribute("type"),
                  &name = packNode.attribute("name");

  const Package::Type packageType = Package::getType(type.value_or(""));
  if(options.rejectUnsupportedPackageTypes && packageType == Package::UnknownType) {
    if(type)
      throw reapack_error("unsupported package type '" + std::string(*type) + "'");
    throw reapack_error("missing package type");
  }

  Package *pack = new Package(packageType, name.value_or(""), cat);
  std::unique_ptr<Package> ptr(pack);

  if(const XmlString &desc = packNode.attribute("desc"))
    pack->setDescription(*desc);

  for(XmlNode node = packNode.firstChild("version");
      node; node = node.nextSibling("version"))
    LoadVersionV1(node, pack, options);

  if(XmlNode node = packNode.firstChild("metadata"))
    LoadMetadataV1(node, pack->metadata());

  if(cat->addPackage(pack))
    ptr.release();
}

void LoadVersionV1(XmlNode verNode, Package *pkg,
  const Index::LoadOptions &options)
{
  const XmlString &name = verNode.attribute("name");
  Version *ver = new Version(name.value_or(""), pkg);
  std::unique_ptr<Version> ptr(ver);

  if(const XmlString &author = verNode.attribute("author"))
    ver->setAuthor(*author);

  if(const XmlString &time = verNode.attribute("time"))
    ver->setTime(*time);

  XmlNode node = verNode.firstChild("source");

  bool hasArm64Ec = false;
#if defined(_WIN32) && defined(_M_ARM64EC)
  while(node) {
    const char *platform = *node.attribute("platform");
    if(platform && Platform(platform) == Platform::Windows_arm64ec) {
      hasArm64Ec = true;
      break;
    }
    node = node.nextSibling("source");
  }
  node = verNode.firstChild("source");
#endif

  while(node) {
    LoadSourceV1(node, ver, hasArm64Ec, options);
    node = node.nextSibling("source");
  }

  node = verNode.firstChild("changelog");

  if(node) {
    if(const XmlString &changelog = node.text())
      ver->setChangelog(*changelog);
  }

  if(pkg->addVersion(ver))
    ptr.release();
}

void LoadSourceV1(XmlNode node, Version *ver, const bool hasArm64Ec,
  const Index::LoadOptions &options)
{
  const XmlString &platform = node.attribute("platform"),
                  &type     = node.attribute("type"),
                  &file     = node.attribute("file"),
                  &checksum = node.attribute("hash"),
                  &main     = node.attribute("main"),
                  &url      = node.text();

  Source *src = new Source(file.value_or(""), url.value_or(""), ver);
  std::unique_ptr<Source> ptr(src);

  const Platform sourcePlatform(platform.value_or("all"), hasArm64Ec);
  if(options.rejectUnsupportedPlatforms &&
      sourcePlatform == Platform::Unknown) {
    throw reapack_error("unsupported source platform '" +
      std::string(*platform) + "'");
  }

  const char *sourceTypeName = type.value_or("");
  const Package::Type sourceType = Package::getType(sourceTypeName);
  if(options.rejectUnsupportedSourceTypes && type &&
      sourceType == Package::UnknownType) {
    if(*sourceTypeName)
      throw reapack_error("unsupported source type '" +
        std::string(sourceTypeName) + "'");
    throw reapack_error("empty source type");
  }

  src->setChecksum(checksum.value_or(""));
  src->setPlatform(sourcePlatform);
  src->setTypeOverride(sourceType);

  int sections = 0;
  std::string section;
  std::istringstream mainStream(main.value_or(""));
  while(std::getline(mainStream, section, '\x20'))
    sections |= Source::getSection(section.c_str());
  src->setSections(sections);

  if(ver->addSource(src))
    ptr.release();
}
