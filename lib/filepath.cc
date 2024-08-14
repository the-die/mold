#include "common.h"

#include <filesystem>
#include <sys/stat.h>

#ifdef __APPLE__
# include <mach-o/dyld.h>
#endif

#ifdef __FreeBSD__
# include <sys/sysctl.h>
#endif

namespace mold {

// std::filesystem::read_symlink
//   https://en.cppreference.com/w/cpp/filesystem/read_symlink
// std::filesystem::path::append, std::filesystem::path::operator/=
//   https://en.cppreference.com/w/cpp/filesystem/path/append
std::string get_realpath(std::string_view path) {
  std::error_code ec;
  std::filesystem::path link = std::filesystem::read_symlink(path, ec);
  if (ec)
    return std::string(path);
  return (filepath(path) / ".." / link).lexically_normal().string();
}

// std::filesystem::path::lexically_normal
//   https://en.cppreference.com/w/cpp/filesystem/path/lexically_normal
//
//
// Removes redundant '/..' or '/.' from a given path.
// The transformation is done purely by lexical processing.
// This function does not access file system.
std::string path_clean(std::string_view path) {
  return filepath(path).lexically_normal().string();
}

// std::filesystem::current_path
//   https://en.cppreference.com/w/cpp/filesystem/current_path
// std::filesystem::absolute
//   https://en.cppreference.com/w/cpp/filesystem/absolute
std::filesystem::path to_abs_path(std::filesystem::path path) {
  if (path.is_absolute())
    return path.lexically_normal();
  return (std::filesystem::current_path() / path).lexically_normal();
}

// Returns the path of the mold executable itself
std::string get_self_path() {
#if __APPLE__ || _WIN32
  fprintf(stderr, "mold: get_self_path is not supported");
  exit(1);
#elif __FreeBSD__
  // /proc may not be mounted on FreeBSD. The proper way to get the
  // current executable's path is to use sysctl(2).
  int mib[4];
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  size_t size;
  sysctl(mib, 4, NULL, &size, NULL, 0);

  std::string path;
  path.resize(size);
  sysctl(mib, 4, path.data(), &size, NULL, 0);
  return path;
#else
  // /proc/self/exe
  //   https://man7.org/linux/man-pages/man5/proc.5.html
  // /proc/pid/exe
  //   Under Linux 2.2 and later, this file is a symbolic link
  //   containing the actual pathname of the executed command.
  //   This symbolic link can be dereferenced normally;
  //   attempting to open it will open the executable.
  return std::filesystem::read_symlink("/proc/self/exe").string();
#endif
}

} // namespace mold
