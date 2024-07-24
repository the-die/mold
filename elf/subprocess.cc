#if !defined(_WIN32) && !defined(__APPLE__)

#include "mold.h"
#include "config.h"

#include <filesystem>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace mold::elf {

#ifdef MOLD_X86_64
// Exiting from a program with large memory usage is slow --
// it may take a few hundred milliseconds. To hide the latency,
// we fork a child and let it do the actual linking work.
std::function<void()> fork_child() {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(1);
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(1);
  }

  if (pid > 0) {
    // Parent
    close(pipefd[1]);

    char buf[1];
    if (read(pipefd[0], buf, 1) == 1)
      _exit(0);

    int status;
    waitpid(pid, &status, 0);

    // WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG
    // https://man7.org/linux/man-pages/man2/waitpid.2.html
    if (WIFEXITED(status))
      _exit(WEXITSTATUS(status));
    if (WIFSIGNALED(status))
      raise(WTERMSIG(status));
    _exit(1);
  }

  // Child
  close(pipefd[0]);

  return [=] {
    char buf[] = {1};
    [[maybe_unused]] int n = write(pipefd[1], buf, 1);
    assert(n == 1);
  };
}
#endif

// This function tries to locate the `mold-wrapper.so` shared object file in three different locations:
//
// In the same directory as the executable.
// In a default directory defined by `MOLD_LIBDIR`.
// One directory up from the executable's directory, in `lib/mold`.
//
// If the file is found in any of these locations, the function returns the path to the file. If the
// file is not found, it logs a fatal error message indicating that the file is missing.
//
//
// see CMakeLists.txt: mold-wrapper
template <typename E>
static std::string find_dso(Context<E> &ctx, std::filesystem::path self) {
  // Look for mold-wrapper.so from the same directory as the executable is.
  std::filesystem::path path = self.parent_path() / "mold-wrapper.so";
  std::error_code ec;
  if (std::filesystem::is_regular_file(path, ec) && !ec)
    return path;

  // If not found, search $(MOLD_LIBDIR)/mold, which is /usr/local/lib/mold
  // by default.
  path = MOLD_LIBDIR "/mold/mold-wrapper.so";
  if (std::filesystem::is_regular_file(path, ec) && !ec)
    return path;

  // Look for ../lib/mold/mold-wrapper.so
  path = self.parent_path() / "../lib/mold/mold-wrapper.so";
  if (std::filesystem::is_regular_file(path, ec) && !ec)
    return path;

  Fatal(ctx) << "mold-wrapper.so is missing";
}

// It is sometimes very hard to pass an appropriate command line option to cc to specify an
// alternative linker. To address this situation, mold has a feature to intercept all invocations of
// ld, ld.bfd, ld.lld, or ld.gold and redirect them to itself. To use this feature, run make (or
// another build command) as a subcommand of mold as follows:
//
// mold -run make <make-options-if-any>
//
// Internally, mold invokes a given command with the LD_PRELOAD environment variable set to its
// companion shared object file. The shared object file intercepts all function calls to
// exec(3)-family functions to replace argv[0] with mold if it is ld, ld.bf, ld.gold, or ld.lld.
template <typename E>
[[noreturn]]
void process_run_subcommand(Context<E> &ctx, int argc, char **argv) {
  assert(argv[1] == "-run"s || argv[1] == "--run"s);

  if (!argv[2])
    Fatal(ctx) << "-run: argument missing";

  // Get the mold-wrapper.so path
  std::string self = get_self_path();
  std::string dso_path = find_dso(ctx, self);

  // LD_PRELOAD
  //    https://man7.org/linux/man-pages/man8/ld.so.8.html
  //
  // A list of additional, user-specified, ELF shared objects
  // to be loaded before all others.  This feature can be used
  // to selectively override functions in other shared objects.
  //
  // The items of the list can be separated by spaces or
  // colons, and there is no support for escaping either
  // separator.  The objects are searched for using the rules
  // given under DESCRIPTION.  Objects are searched for and
  // added to the link map in the left-to-right order specified
  // in the list.
  //
  // In secure-execution mode, preload pathnames containing
  // slashes are ignored.  Furthermore, shared objects are
  // preloaded only from the standard search directories and
  // only if they have set-user-ID mode bit enabled (which is
  // not typical).
  //
  // Within the names specified in the LD_PRELOAD list, the
  // dynamic linker understands the tokens $ORIGIN, $LIB, and
  // $PLATFORM (or the versions using curly braces around the
  // names) as described above in Dynamic string tokens.  (See
  // also the discussion of quoting under the description of
  // LD_LIBRARY_PATH.)
  //
  // There are various methods of specifying libraries to be
  // preloaded, and these are handled in the following order:
  //
  // (1)  The LD_PRELOAD environment variable.
  //
  // (2)  The --preload command-line option when invoking the
  //      dynamic linker directly.
  //
  // (3)  The /etc/ld.so.preload file.
  //
  //
  // putenv
  //    https://man7.org/linux/man-pages/man3/putenv.3.html
  //
  //
  // see CMakeLists.txt: mold-wrapper
  //
  //
  // Set environment variables
  putenv(strdup(("LD_PRELOAD=" + dso_path).c_str()));
  putenv(strdup(("MOLD_PATH=" + self).c_str()));

  // argv[0]: mold, argv[1]: -run/--run
  //
  // If ld, ld.lld or ld.gold is specified, run mold itself
  if (std::string cmd = filepath(argv[2]).filename();
      cmd == "ld" || cmd == "ld.lld" || cmd == "ld.gold") {
    std::vector<char *> args;
    args.push_back(argv[0]);
    args.insert(args.end(), argv + 3, argv + argc);
    args.push_back(nullptr);
    // execv
    //    https://man7.org/linux/man-pages/man3/exec.3.html
    //    https://www.gnu.org/software/libc/manual/html_node/Executing-a-File.html
    execv(self.c_str(), args.data());
    Fatal(ctx) << "mold -run failed: " << self << ": " << errno_string();
  }

  // execv
  //    https://man7.org/linux/man-pages/man3/exec.3.html
  //    https://www.gnu.org/software/libc/manual/html_node/Executing-a-File.html
  //
  //
  // Execute a given command
  execvp(argv[2], argv + 2);
  Fatal(ctx) << "mold -run failed: " << argv[2] << ": " << errno_string();
}

using E = MOLD_TARGET;

template void process_run_subcommand(Context<E> &, int, char **);

} // namespace mold::elf

#endif
