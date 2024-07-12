#include <execinfo.h>  // For backtrace()
#include <fcntl.h>     // for open()
#include <unistd.h>    // for sleep()

#include <cassert>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

static constexpr size_t MAX_STACKTRACE_DEPTH = 64;

void WriteStackTrace(int file_descriptor) {
  void* trace[MAX_STACKTRACE_DEPTH];
  size_t trace_depth = backtrace(trace, MAX_STACKTRACE_DEPTH);
  // Note that we skip the first frame here so this function won't show up in
  // the printed trace.
  backtrace_symbols_fd(trace + 1, trace_depth - 1, file_descriptor);
}

/******************************************************************************/
void FatalSignalHandler(int signal) {
  // Restore the default signal handler for SIGSEGV in case another one
  // happens, and for the re-issue below.
  std::signal(signal, SIG_DFL);

  // WriteStackTrace should theoretically be safe to call here.
  static constexpr const char error_msg[] =
      "*** FatalSignalHandler stack trace: ***\n";
  // Write using safe `write` function. Skip null character.
  if (write(fileno(stderr), error_msg, sizeof(error_msg) - 1) > 0) {
    WriteStackTrace(fileno(stderr));
  }

  // Give dummy function time to run. Use "safe" sleep() function.
  sleep(1);

  // Now that the signal handler has been removed we can simply return. If
  // SIGSEGV/SIGABRT was triggered by an instruction, it will occur again. This
  // time it will be handled by the default handler which triggers a core dump.
  return;
}

int main() {
  // Dummy thread to run in background.
  std::thread t1([]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::cout << "thread..." << std::endl;
    }
  });

  // Use our function to handle segmentation faults.
  // Could add additional signals like: SIGSEGV, SIGSYS, etc.
  std::signal(SIGSEGV, FatalSignalHandler);

  std::cout << "Hello World." << std::endl;

  // Trigger segfault by writing to nullptr.
  char* BAD_PTR = nullptr;
  *BAD_PTR = 10;

  std::cout << "Finished." << std::endl;

  return 0;
}
