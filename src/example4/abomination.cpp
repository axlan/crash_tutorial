#include <unistd.h>  // for sleep()

#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <thread>

// Used to sync between the FatalSignalHandler and SignalMonitorFunc.
static volatile std::atomic<int> fatal_fault_signal = 0;

// Time to allow a test to run before declaring it timedout.
static constexpr auto TEST_TIMEOUT = std::chrono::milliseconds(200);

// Have signals set the flag then sleep forever.
void FatalSignalHandler(int signal) {
  fatal_fault_signal = signal;
  while (true) {
    sleep(100);
  }
}

// Struct to report test results.
enum class TestResult { EXITED, TIMED_OUT, CRASH };
const char* TestResultToString(TestResult result) {
  switch (result) {
    case TestResult::EXITED:
      return "EXITED";
    case TestResult::TIMED_OUT:
      return "TIMED_OUT";
    case TestResult::CRASH:
      return "CRASH";
  }
  return "INVALID";
}

TestResult CheckIfAborts(const std::function<void()>& func) {
  // Clear the fault flag.
  fatal_fault_signal = 0;
  // For checking if the function completed.
  std::atomic<bool> complete = false;

  // Try running the function in a new thread.
  std::thread test_thread([&complete, func]() {
    func();
    complete = true;
  });

  // Poll the flags to check if the function crashed or completed.
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < (start + TEST_TIMEOUT)) {
    if (complete) {
      test_thread.join();
      return TestResult::EXITED;
    } else if (fatal_fault_signal) {
      // Thread is stuck so abandon it.
      test_thread.detach();
      return TestResult::CRASH;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Thread is stuck so abandon it. Could call
  // pthread_cancel(test_thread.native_handle()), but I'm not sure the tradeoffs
  // against just letting the thread run.
  test_thread.detach();
  return TestResult::TIMED_OUT;
}

int main(int argc, char* argv[]) {
  // Use FatalSignalHandler to handle aborts.
  // Could add additional signals like: SIGSEGV, SIGSYS, etc.
  std::signal(SIGABRT, FatalSignalHandler);

  std::cout << "Test 1: "
            << TestResultToString(CheckIfAborts([]() { abort(); }))
            << std::endl;

  std::cout << "Test 2: "
            << TestResultToString(CheckIfAborts([]() { assert(false); }))
            << std::endl;

  std::cout << "Test 3: "
            << TestResultToString(CheckIfAborts([]() { sleep(100); }))
            << std::endl;

  std::cout << "Test 4: " << TestResultToString(CheckIfAborts([]() {}))
            << std::endl;

  return 0;
}
