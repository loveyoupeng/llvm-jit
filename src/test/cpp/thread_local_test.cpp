#include <gtest/gtest.h>
#include <thread>

thread_local int value = 0;
TEST(ThreadLocal, test) {

  std::thread t{[&]() {
    ASSERT_EQ(0, value);
    for (int i = 0; i < 10; ++i) {
      value += i;
    }
    ASSERT_EQ(45, value);
  }};

  t.join();
  ASSERT_EQ(0, value);
}
