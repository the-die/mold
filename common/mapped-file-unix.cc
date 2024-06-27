#include "common.h"

namespace mold {

MappedFile *open_file_impl(const std::string &path, std::string &error) {
  // open
  //   https://man7.org/linux/man-pages/man2/open.2.html
  //   https://man7.org/linux/man-pages/man3/open.3p.html
  i64 fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    // ENOENT O_CREAT is not set and the named file does not exist.
    //
    // ENOENT A directory component in pathname does not exist or is a
    //        dangling symbolic link.
    //
    // ENOENT pathname refers to a nonexistent directory, O_TMPFILE and
    //        one of O_WRONLY or O_RDWR were specified in flags, but
    //        this kernel version does not provide the O_TMPFILE
    //        functionality.
    if (errno != ENOENT)
      error = "opening " + path + " failed: " + errno_string();
    return nullptr;
  }

  // fstat
  //   https://man7.org/linux/man-pages/man2/stat.2.html
  //   https://man7.org/linux/man-pages/man3/fstat.3p.html
  struct stat st;
  if (fstat(fd, &st) == -1)
    error = path + ": fstat failed: " + errno_string();

  MappedFile *mf = new MappedFile;
  mf->name = path;
  mf->size = st.st_size;

  if (st.st_size > 0) {
    // mmap
    //   https://man7.org/linux/man-pages/man2/mmap.2.html
    //   https://man7.org/linux/man-pages/man3/mmap.3p.html
    //
    // MAP_PRIVATE
    //   Create a private copy-on-write mapping.  Updates to the
    //   mapping are not visible to other processes mapping the
    //   same file, and are not carried through to the underlying
    //   file.  It is unspecified whether changes made to the file
    //   after the mmap() call are visible in the mapped region.
    mf->data = (u8 *)mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE, fd, 0);
    if (mf->data == MAP_FAILED)
      error = path + ": mmap failed: " + errno_string();
  }

  // After the mmap() call has returned, the file descriptor, fd, can
  // be closed immediately without invalidating the mapping.
  close(fd);
  return mf;
}

void MappedFile::unmap() {
  if (size == 0 || parent || !data)
    return;
  // unmap
  //   https://man7.org/linux/man-pages/man2/mmap.2.html
  //   https://man7.org/linux/man-pages/man3/munmap.3p.html
  munmap(data, size);
  data = nullptr;
}

void MappedFile::close_fd() {
  if (fd == -1)
    return;
  close(fd);
  fd = -1;
}

void MappedFile::reopen_fd(const std::string &path) {
  if (fd == -1)
    fd = open(path.c_str(), O_RDONLY);
}

} // namespace mold
