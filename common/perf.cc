#include "common.h"

#include <functional>
#include <iomanip>
#include <ios>

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/time.h>
#endif

namespace mold {

i64 Counter::get_value() {
  // https://oneapi-spec.uxlfoundation.org/specifications/oneapi/v1.3-rev-1/elements/onetbb/source/thread_local_storage/enumerable_thread_specific_cls/combining
  // Computes reduction over all elements using binary functor f. If there are no elements, creates
  // the result using the same rules as for creating a thread-local element.
  return values.combine(std::plus());
}

void Counter::print() {
  sort(instances, [](Counter *a, Counter *b) {
    return a->get_value() > b->get_value();
  });

  for (Counter *c : instances)
    std::cout << std::setw(20) << std::right << c->name
              << "=" << c->get_value() << "\n";
}

static i64 now_nsec() {
  // https://en.cppreference.com/w/cpp/chrono/steady_clock
  return (i64)std::chrono::steady_clock::now().time_since_epoch().count();
}

static std::pair<i64, i64> get_usage() {
#ifdef _WIN32
  auto to_nsec = [](FILETIME t) -> i64 {
    return (((u64)t.dwHighDateTime << 32) + (u64)t.dwLowDateTime) * 100;
  };

  FILETIME creation, exit, kernel, user;
  GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);
  return {to_nsec(user), to_nsec(kernel)};
#else
  // timeval
  //   https://man7.org/linux/man-pages/man3/timeval.3type.html
  //
  //   struct timeval {
  //       time_t       tv_sec;   /* Seconds */
  //       suseconds_t  tv_usec;  /* Microseconds */
  //   };
  auto to_nsec = [](struct timeval t) -> i64 {
    // https://en.cppreference.com/w/cpp/language/integer_literal
    //
    // Optional single quotes (') may be inserted between the digits as a separator; they are
    // ignored when determining the value of the literal.
    return (i64)t.tv_sec * 1'000'000'000 + t.tv_usec * 1'000;
  };

  // getrusage
  //   https://man7.org/linux/man-pages/man2/getrusage.2.html
  //
  // ru_utime
  //   This is the total amount of time spent executing in user
  //   mode, expressed in a timeval structure (seconds plus
  //   microseconds).
  //
  // ru_stime
  //   This is the total amount of time spent executing in kernel
  //   mode, expressed in a timeval structure (seconds plus
  //   microseconds).
  struct rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  return {to_nsec(ru.ru_utime), to_nsec(ru.ru_stime)};
#endif
}

TimerRecord::TimerRecord(std::string name, TimerRecord *parent)
  : name(name), parent(parent) {
  start = now_nsec();
  std::tie(user, sys) = get_usage();
  if (parent)
    parent->children.push_back(this);
}

void TimerRecord::stop() {
  if (stopped)
    return;
  stopped = true;

  i64 user2;
  i64 sys2;
  std::tie(user2, sys2) = get_usage();

  end = now_nsec();
  user = user2 - user;
  sys = sys2 - sys;
}

static void print_rec(TimerRecord &rec, i64 indent) {
  printf(" % 8.3f % 8.3f % 8.3f  %s%s\n",
         ((double)rec.user / 1'000'000'000),
         ((double)rec.sys / 1'000'000'000),
         (((double)rec.end - rec.start) / 1'000'000'000),
         std::string(indent * 2, ' ').c_str(),
         rec.name.c_str());

  sort(rec.children, [](TimerRecord *a, TimerRecord *b) {
    return a->start < b->start;
  });

  for (TimerRecord *child : rec.children)
    print_rec(*child, indent + 1);
}

void print_timer_records(
    tbb::concurrent_vector<std::unique_ptr<TimerRecord>> &records) {
  for (i64 i = records.size() - 1; i >= 0; i--)
    records[i]->stop();

  for (i64 i = 0; i < records.size(); i++) {
    TimerRecord &inner = *records[i];
    if (inner.parent)
      continue;

    for (i64 j = i - 1; j >= 0; j--) {
      TimerRecord &outer = *records[j];
      if (outer.start <= inner.start && inner.end <= outer.end) {
        inner.parent = &outer;
        outer.children.push_back(&inner);
        break;
      }
    }
  }

  std::cout << "     User   System     Real  Name\n";

  for (std::unique_ptr<TimerRecord> &rec : records)
    if (!rec->parent)
      print_rec(*rec, 0);

  std::cout << std::flush;
}

} // namespace mold
