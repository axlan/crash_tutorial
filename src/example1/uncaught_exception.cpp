#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

void my_terminate_handler() {
  // Get the string for the last exception.
  std::exception_ptr ex_ptr = std::current_exception();
  try {
    std::rethrow_exception(ex_ptr);
  } catch (std::exception &ex) {
    std::cout << "Terminated due to exception: " << ex.what() << std::endl;
  }
  // Give some time for the dummy thread to run.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
  // Dummy thread to run in background.
  std::thread t1([]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::cout << "thread..." << std::endl;
    }
  });

  // Call this function when an exception would terminate the process.
  std::set_terminate(my_terminate_handler);

  std::cout << "Hello World." << std::endl;

  // Trow an unhandled exception.
  throw std::runtime_error("Something went wrong");

  std::cout << "Finished." << std::endl;

  return 0;
}
