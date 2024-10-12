#define _GNU_SOURCE 1

#include <dlfcn.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __has_include(<alloca.h>)
# include <alloca.h>
#endif

// environ
//    https://www.man7.org/linux/man-pages/man7/environ.7.html
//    https://www.man7.org/linux/man-pages/man3/environ.3p.html
//
// The variable environ points to an array of pointers to strings
// called the "environment".  The last pointer in this array has the
// value NULL.  This array of strings is made available to the
// process by the execve(2) call when a new program is started.
// When a child process is created via fork(2), it inherits a copy
// of its parent's environment.
extern char **environ;

// see `process_run_subcommand`
static char *get_mold_path() {
  // getenv
  //    https://man7.org/linux/man-pages/man3/getenv.3.html
  char *path = getenv("MOLD_PATH");
  if (path)
    return path;
  fprintf(stderr, "MOLD_PATH is not set\n");
  exit(1);
}

static void debug_print(const char *fmt, ...) {
  if (!getenv("MOLD_WRAPPER_DEBUG"))
    return;

  // fflush
  //    https://man7.org/linux/man-pages/man3/fflush.3.html
  //
  // stderr
  //    https://man7.org/linux/man-pages/man3/stdin.3.html
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "mold-wrapper.so: ");
  vfprintf(stderr, fmt, ap);
  fflush(stderr);
  va_end(ap);
}

// va_start, va_arg, va_end, va_copy
//    https://man7.org/linux/man-pages/man3/stdarg.3.html
static int count_args(va_list *ap) {
  va_list aq;
  va_copy(aq, *ap);

  int i = 0;
  while (va_arg(aq, char *))
    i++;
  va_end(aq);
  return i;
}

// argv[0]: arg0
// argv[i]: va_arg(ap, char*)
// argv[n]: NULL
//
// exhaust ap
static void copy_args(char **argv, const char *arg0, va_list *ap) {
  int i = 1;
  char *arg;
  while ((arg = va_arg(*ap, char *)))
    argv[i++] = arg;

  ((const char **)argv)[0] = arg0;
  ((const char **)argv)[i] = NULL;
}

// example: .../ld, ld
static bool is_ld(const char *path) {
  const char *ptr = path + strlen(path);
  while (path < ptr && ptr[-1] != '/')
    ptr--;

  return !strcmp(ptr, "ld") || !strcmp(ptr, "ld.lld") ||
         !strcmp(ptr, "ld.gold") || !strcmp(ptr, "ld.bfd") ||
         !strcmp(ptr, "ld.mold");
}

int execvpe(const char *file, char *const *argv, char *const *envp) {
  debug_print("execvpe %s\n", file);

  if (!strcmp(file, "ld") || is_ld(file))
    file = get_mold_path();

  // putenv
  //    https://man7.org/linux/man-pages/man3/putenv.3.html
  //    https://www.man7.org/linux/man-pages/man3/putenv.3p.html
  //
  // The putenv() function shall use the string argument to set
  // environment variable values. The string argument should point to
  // a string of the form "name=value".  The putenv() function shall
  // make the value of the environment variable name equal to value by
  // altering an existing variable or creating a new one. In either
  // case, the string pointed to by string shall become part of the
  // environment, so altering the string shall change the environment.
  //
  // The putenv() function need not be thread-safe.
  for (int i = 0; envp[i]; i++)
    putenv(envp[i]);

  // typeof
  //    https://gcc.gnu.org/onlinedocs/gcc/Typeof.html
  //
  //
  // dlsym
  //    https://man7.org/linux/man-pages/man3/dlsym.3.html
  //    https://man7.org/linux/man-pages/man3/dlsym.3p.html
  //
  // RTLD_NEXT
  //
  // Find the next occurrence of the desired symbol in the
  // search order after the current object.  This allows one to
  // provide a wrapper around a function in another shared
  // object, so that, for example, the definition of a function
  // in a preloaded shared object (see LD_PRELOAD in ld.so(8))
  // can find and invoke the "real" function provided in
  // another shared object (or for that matter, the "next"
  // definition of the function in cases where there are
  // multiple layers of preloading).
  typeof(execvpe) *real = dlsym(RTLD_NEXT, "execvp");
  return real(file, argv, environ);
}

int execve(const char *path, char *const *argv, char *const *envp) {
  debug_print("execve %s\n", path);
  if (is_ld(path))
    path = get_mold_path();
  typeof(execve) *real = dlsym(RTLD_NEXT, "execve");
  return real(path, argv, envp);
}

int execl(const char *path, const char *arg0, ...) {
  va_list ap;
  va_start(ap, arg0);
  // add 2 for arg0 and NULL
  char **argv = alloca((count_args(&ap) + 2) * sizeof(char *));
  copy_args(argv, arg0, &ap);
  va_end(ap);
  return execve(path, argv, environ);
}

int execlp(const char *file, const char *arg0, ...) {
  va_list ap;
  va_start(ap, arg0);
  // add 2 for arg0 and NULL
  char **argv = alloca((count_args(&ap) + 2) * sizeof(char *));
  copy_args(argv, arg0, &ap);
  va_end(ap);
  return execvpe(file, argv, environ);
}

// int execle(const char *pathname, const char *arg, ...
//            /*, (char *) NULL, char *const envp[] */);
int execle(const char *path, const char *arg0, ...) {
  va_list ap;
  va_start(ap, arg0);
  // add 2 for arg0 and NULL
  char **argv = alloca((count_args(&ap) + 2) * sizeof(char *));
  copy_args(argv, arg0, &ap);
  char **env = va_arg(ap, char **);
  va_end(ap);
  return execve(path, argv, env);
}

int execv(const char *path, char *const *argv) {
  return execve(path, argv, environ);
}

int execvp(const char *file, char *const *argv) {
  return execvpe(file, argv, environ);
}

// posix_spawn, posix_spawnp
//    https://man7.org/linux/man-pages/man3/posix_spawn.3.html
//
// The posix_spawn() and posix_spawnp() functions are used to create
// a new child process that executes a specified file.  These
// functions were specified by POSIX to provide a standardized
// method of creating new processes on machines that lack the
// capability to support the fork(2) system call.  These machines
// are generally small, embedded systems lacking MMU support.
//
// The only difference between posix_spawn() and posix_spawnp() is
// the manner in which they specify the file to be executed by the
// child process.  With posix_spawn(), the executable file is
// specified as a pathname (which can be absolute or relative).
// With posix_spawnp(), the executable file is specified as a simple
// filename; the system searches for this file in the list of
// directories specified by PATH (in the same way as for execvp(3)).
int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp,
                char *const *argv, char *const *envp) {
  debug_print("posix_spawn %s\n", path);
  if (is_ld(path))
    path = get_mold_path();
  typeof(posix_spawn) *real = dlsym(RTLD_NEXT, "posix_spawn");
  return real(pid, path, file_actions, attrp, argv, envp);
}

int posix_spawnp(pid_t *pid, const char *file,
		 const posix_spawn_file_actions_t *file_actions,
		 const posix_spawnattr_t *attrp,
		 char *const *argv, char *const *envp) {
  debug_print("posix_spawnp %s\n", file);
  if (is_ld(file))
    file = get_mold_path();
  typeof(posix_spawnp) *real = dlsym(RTLD_NEXT, "posix_spawnp");
  return real(pid, file, file_actions, attrp, argv, envp);
}
