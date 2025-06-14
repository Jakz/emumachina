#pragma once

#include "path.h"
#include "common.h"

#include <vector>

class FileSystem
{
private:
  bool internalDeleteDirectory(const path& path) const;

public:
  static const FileSystem* i();
  
  void scanFolder(const path& root, const std::function<void(bool, const path& path)>& lambda, bool recursive = true);
  std::vector<path> contentsOfFolder(const path& folder, bool recursive = true, std::function<bool(path)> exclude = [](const path&){ return false; }) const;
  
  bool existsAsFolder(const path& path) const;
  bool existsAsFile(const path& path) const;
  
  bool copy(const path& from, const path& to) const;
  
  bool createFolder(const path& folder, bool intermediate = true) const;
  
  bool deleteFile(const path& path) const;
  
  bool fallocate(const path& path, size_t size) const;
};
