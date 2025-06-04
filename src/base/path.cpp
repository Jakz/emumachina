#include "path.h"

#include "common.h"

#include "file_system.h"
#include <filesystem>

namespace fs = std::filesystem;
static constexpr const char SEPARATOR = '/';

path::path(const char* data) : _data(data)
{
  /* replace windows separator to other separator */
  std::replace(_data.begin(), _data.end(), '\\', SEPARATOR);
  
  if (_data.length() > 1 && _data.back() == SEPARATOR)
    _data.pop_back();
}

path::path(const std::string& data) : _data(data)
{
  if (!_data.empty() && _data.back() == SEPARATOR && _data.length() > 1)
    _data.pop_back();
}

path::path(const std::filesystem::path& fspath) : path(fspath.string())
{

}

bool path::isAbsolute() const
{
  return !_data.empty() && _data[0] == SEPARATOR;
}

std::vector<path> path::contents() const
{
  return FileSystem::i()->contentsOfFolder(*this);
}

bool path::isFolder() const { return FileSystem::i()->existsAsFolder(*this); }

bool path::exists() const
{
  return FileSystem::i()->existsAsFile(*this) || FileSystem::i()->existsAsFolder(*this);
}

path path::absolute() const
{
  return path(fs::absolute(_data));
}

path path::relativizeToParent(const path& parent) const
{
  return path(_data.substr(parent._data.length()+1));
}

path path::relativizeChildren(const path& children) const
{
  return path(children._data.substr(_data.length()+1));
}

std::string path::filename() const
{
  size_t index = _data.find_last_of(SEPARATOR);

  return index != std::string::npos ? _data.substr(index+1) : _data;
}

std::string path::extension() const
{
  size_t index = _data.find_last_of('.');

  if (index == std::string::npos)
    return "";

  size_t sepIndex = _data.find_last_of(SEPARATOR);
  if (sepIndex != std::string::npos && sepIndex > index)
    return "";

  return _data.substr(index + 1);

}

std::string path::filenameWithoutExtension() const
{
  auto filename = this->filename();
  size_t index = filename.find_last_of('.');
  return index != std::string::npos ? filename.substr(0, index) : filename;
}

size_t path::length() const
{
  if (exists() && !isFolder())
    return fs::file_size(_data);
  else
    return 0;
}

size_t path::writeAll(const void* data, size_t count, size_t size) const
{
  FILE* out = fopen(_data.c_str(), "wb");

  if (out)
  {
    size_t written = fwrite(data, size, count, out);
    fclose(out);
    return written;
  }

  return 0;
}

path path::withExtension(const path_extension& extension) const
{
  return parent() + (filenameWithoutExtension() + "." + extension);
}

bool endsWith(const std::string& str, char c) { return str.back() == c; }
bool startsWith(const std::string& str, char c) { return str.front() == c; }
path path::append(const path& other) const
{
  if (other.isAbsolute())
    return *this;

  if (_data.empty())
    return other;
  else if (!endsWith(_data,SEPARATOR) && !startsWith(other._data, SEPARATOR))
    return path(_data + SEPARATOR + other._data);
  else if (endsWith(_data, SEPARATOR) && startsWith(other._data, SEPARATOR))
    return path(_data + other._data.substr(1));
  else
    return path(_data + other._data);
}

bool path::hasExtension(const std::string& ext) const
{
  size_t index = _data.find_last_of('.');
  return index != std::string::npos && _data.substr(index+1) == ext;
}

path path::removeLast() const
{
  size_t index = _data.rfind(SEPARATOR);
  
  if (index != std::string::npos)
    return path(_data.substr(0, index));
  else
    return path();
}

std::pair<path, path> path::splitParentAndFilename() const
{
  size_t index = _data.rfind(SEPARATOR);
  
  if (index == std::string::npos)
    return std::make_pair(path(), *this);
  else
    return std::make_pair(_data.substr(0, index), _data.substr(index+1));
}

path path::removeAllButFirst() const
{
  size_t index = _data.find(SEPARATOR);
  return index != std::string::npos ? _data.substr(index+1) : _data;
}

path path::makeRelative() const
{
  return _data[0] == SEPARATOR ? path(&_data[1]) : *this;
}

