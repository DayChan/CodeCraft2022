#include <chrono>
#include <thread>
#include <iostream>

class Timer {
  public:
    Timer() {
      start_ = std::chrono::system_clock::now();
    }

    void set_timeout(double timeout) {  // 毫秒
      std::thread t(&Timer::inner_func, this, timeout);
      t.detach();
    }

  private:
    void inner_func(double timeout) {
      while (1) {
        double pass_time = (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_).count();
        if (pass_time >= timeout) {
            std::cout << "process exit, pass time = " << pass_time << "ms" << std::endl;
            exit(0);
        }
      };
    }
    
    std::chrono::system_clock::time_point start_;
};