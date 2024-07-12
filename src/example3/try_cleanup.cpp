#include <execinfo.h>  // For backtrace()
#include <fcntl.h>     // for open()
#include <unistd.h>    // for sleep()

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <functional>
#include <iostream>
#include <thread>

static constexpr size_t MAX_STACKTRACE_DEPTH = 64;
static constexpr unsigned FATAL_CALLBACK_TIMEOUT_SEC = 3;

// Used to sync between the FatalSignalHandler and SignalMonitorFunc.
static volatile std::atomic<int> fatal_fault_signal = 0;

enum class CleanupType { SUCCEED, BLOCK, CRASH };

void WriteStackTrace(int file_descriptor) {
  void* trace[MAX_STACKTRACE_DEPTH];
  size_t trace_depth = backtrace(trace, MAX_STACKTRACE_DEPTH);
  // Note that we skip the first frame here so this function won't show up in
  // the printed trace.
  backtrace_symbols_fd(trace + 1, trace_depth - 1, file_descriptor);
}

void FatalSignalHandler(int signal) {
  // WriteStackTrace should theoretically be safe to call here.
  static constexpr const char error_msg[] =
      "*** FatalSignalHandler stack trace: ***\n";
  // Write using safe `write` function. Skip null character.
  if (write(fileno(stderr), error_msg, sizeof(error_msg) - 1) > 0) {
    WriteStackTrace(fileno(stderr));
  }

  // Signal to the monitor thread that a fatal error occurred.
  fatal_fault_signal = signal;

  // Give the signal_handler_thread_ time to cleanup.
  // Only `sleep` is considered safe, so use it in loop.
  for (size_t i = 0; i < FATAL_CALLBACK_TIMEOUT_SEC && fatal_fault_signal != 0;
       i++) {
    sleep(1);
  }

  // Restore the default signal handler for SIGSEGV.
  std::signal(signal, SIG_DFL);
  // Now that the signal handler has been removed we can simply return. If
  // SIGSEGV/SIGABRT was triggered by an instruction, it will occur again. This
  // time it will be handled by the default handler which triggers a core dump.
  return;
}

void SignalMonitorFunc(std::function<void()> shutdown_callback) {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (fatal_fault_signal != 0) {
      std::cout << "Fault detected, shutting down." << std::endl;
      shutdown_callback();
      std::cout << "Shutdown complete." << std::endl;
      fatal_fault_signal = 0;
      break;
    }
  }
}

int main(int argc, char* argv[]) {
  // Condition variable to notify dummy thread to cleanup.
  std::condition_variable cv;
  std::mutex cv_m;

  // Choose cleanup action based on number of CLI args.
  CleanupType cleanup_action = CleanupType::SUCCEED;
  if (argc == 2) {
    cleanup_action = CleanupType::BLOCK;
  } else if (argc == 3) {
    cleanup_action = CleanupType::CRASH;
  }

  // Dummy thread to run in background.
  std::thread dummy_thread([&cv, &cv_m, cleanup_action]() {
    // Loop until notified by condition variable.
    std::unique_lock<std::mutex> lk(cv_m);
    while (cv.wait_for(lk, std::chrono::milliseconds(500)) ==
           std::cv_status::timeout) {
      std::cout << "thread..." << std::endl;
    }

    // Do cleanup action from the CLI arg count.
    assert(cleanup_action != CleanupType::CRASH);
    if (cleanup_action == CleanupType::BLOCK) {
      std::cout << "Cleanup blocked." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(100));
    }
    std::cout << "Dummy exiting." << std::endl;
  });

  // Monitor thread to run in background.
  std::thread monitor_thread(
      &SignalMonitorFunc,
      // Trigger this cleanup callback when a fatal error is detected.
      [&cv, &dummy_thread]() {
        cv.notify_all();
        dummy_thread.join();
      });

  // Use FatalSignalHandler to handle aborts.
  // Could add additional signals like: SIGSYS, etc.
  std::signal(SIGSEGV, FatalSignalHandler);
  std::signal(SIGABRT, FatalSignalHandler);

  // Give threads time to start up.
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  std::cout << "Hello World." << std::endl;

  // Trigger segfault by writing to nullptr.
  char* BAD_PTR = nullptr;
  *BAD_PTR = 10;

  std::cout << "Finished." << std::endl;

  return 0;
}
