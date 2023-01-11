// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>

using namespace verona::cpp;

namespace DiningPhils {
  struct Fork
  {
    const size_t hunger;
    size_t uses;

    Fork(size_t hunger): hunger(hunger), uses(0) {}

    void use()
    {
      ++uses;
    }

    ~Fork() {
      check(uses == hunger * 2);
    }
  };

  struct Philosopher
  {
    cown_ptr<Fork> left;
    cown_ptr<Fork> right;
    size_t hunger;

    Philosopher(cown_ptr<Fork> left, cown_ptr<Fork> right, int hunger)
    : left(left), right(right), hunger(hunger)
    {}

    static void eat(std::unique_ptr<Philosopher> phil)
    {
      if (phil->hunger > 0)
      {
        when(phil->left, phil->right) << [phil = std::move(phil)](acquired_cown<Fork> left, acquired_cown<Fork> right) mutable {
          left->use();
          right->use();
          phil->hunger--;
          eat(std::move(phil));
        };
      }
    }
  };

  static void run() {
    int hunger = 10;

    cown_ptr<Fork> first = make_cown<Fork>(hunger);
    cown_ptr<Fork> fork = first;
    for (int i = 0; i < 4; ++i)
      Philosopher::eat(std::make_unique<Philosopher>(std::exchange(fork, make_cown<Fork>(hunger)), fork, hunger));
    Philosopher::eat(std::make_unique<Philosopher>(fork, first, hunger));
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(DiningPhils::run);
}