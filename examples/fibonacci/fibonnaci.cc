// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace Fib {
  /*
   * A divide and conquer approach to generating values for the fibonacci sequence:
   *   - sequential generates values for n < 4 fib numbers sequentially
   *   - parallel recursively spawns behaviours to solves sub-problems and creates a behaviour
   *     that joins the results once they are ready
   */

  int sequential(int n)
  {
    return n <= 1 ? n : Fib::sequential(n - 1) + Fib::sequential(n - 2);
  }

  cown_ptr<int> parallel(int n)
  {
    if (n <= 4) {
      cown_ptr<int> result = make_cown<int>(int{0});
      when (result) << [n](acquired_cown<int> result) { *result = Fib::sequential(n); };
      return result;
    } else {
      cown_ptr<int> f1 = Fib::parallel(n - 1);
      cown_ptr<int> f2 = Fib::parallel(n - 2);
      when(f1, f2) << [](acquired_cown<int> f1, acquired_cown<int> f2) { *f1 += * f2; };
      return f1;
    }
  }

  void run()
  {
    verona::rt::schedule_lambda([](){
      when(Fib::parallel(1)) << [](acquired_cown<int> result) { check(*result == 1); };
      when(Fib::parallel(10)) << [](acquired_cown<int> result) { check(*result == 55); };
      when(Fib::parallel(15)) << [](acquired_cown<int> result) { check(*result == 610); };
    });
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(Fib::run);
}
