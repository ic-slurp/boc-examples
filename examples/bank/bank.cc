// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace Bank
{
  /*
   * A collection of illustrative examples for:
   * - creating and accessing cowns
   * - spawning behaviours behaviours using 'when'
   */

  struct Account {
    int balance;
    bool frozen;

    Account(int balance): balance(balance), frozen(false) {}
  };

  namespace AccessViolation {
    /*
     * - A cown with contents T is constructed with make_cown<T>.
     * - This either constructs a T in place or constructs a cown from a T value,
     *   returning a cown_ptr<T>.
     * - The cown_ptr<T> cannot be directly dereferenced to access the contents.
     */
    void run() {
      cown_ptr<Account> account = make_cown<Account>(100);
      // account->amount -= 100; illegal access of cown state
    }
  }

  namespace SchedulingWork {
    /*
     * - 'when' must be used to access the contents of a cown.
     * - 'when' takes a list of required cowns and a closure which must have the associated
     *   cown parameters, the cown arguments will be provided when the behaviour is dispatched.
     * - The contents of a required cown_ptr<T> is accessible through an acquired_cown<T>
     *   inside the closure.
     * - When the cowns are availble, the behaviour will be dispatched.
     * - The closure should only capture isolated data.
     *
     * - The following transfer spawns two independent behaviours that require src and dst,
     *   these behaviours may execute in either order or concurrently.
     */
    void transfer(cown_ptr<Account> src, cown_ptr<Account> dst, int amount) {
      when(src) << [amount](acquired_cown<Account> src) { src->balance -= amount; };
      when(dst) << [amount](acquired_cown<Account> dst) { dst->balance += amount; };
    }
  }

  namespace NestingBehaviours {
    /*
     * - Behaviours can be spawned within behaviours.
     * - The spawned behaviour will execute independently and does not have access
     *   to the cowns acquired by the spawning behaviour.
     * - The spawning behaviour does not wait for the spawned behaviour to execute.
     *
     * - In this transfer, the deposit into dst is only spawned if src has enough funds.
     * - After spawning the deposit, the withdraw behaviour terminates.
     * - The deposit does not have access to src.
     */
    void transfer(cown_ptr<Account> src, cown_ptr<Account> dst, int amount) {
      when(src) << [dst, amount](acquired_cown<Account> src) {
        if (src->balance >= amount) {
          src->balance -= amount;
          when(dst) << [amount](acquired_cown<Account> dst) { dst->balance += amount; };
        }
      };
    }
  }

  namespace AtomicTransfer {
    void transfer(cown_ptr<Account> src, cown_ptr<Account> dst, int amount) {
      when(src, dst) << [amount](acquired_cown<Account> src, acquired_cown<Account> dst) {
        if (src->balance >= amount && !src->frozen && !dst->frozen) {
          src->balance -= amount;
          dst->balance += amount;
        }
      };
    }

    void run() {
      cown_ptr<Account> src = make_cown<Account>(100);
      cown_ptr<Account> dst = make_cown<Account>(0);

      transfer(src, dst, 50);

      when(src, dst) << [](acquired_cown<Account> src, acquired_cown<Account> dst) {
        check(src->balance == 50);
        check(dst->balance == 50);
      };
    }
  }

  namespace OrderingOperations {
    void run() {
      cown_ptr<Account> src = make_cown<Account>(0);
      cown_ptr<Account> dst = make_cown<Account>(0);

      when(src) << [](acquired_cown<Account> src) { src->balance += 100; };

      when(dst) << [](acquired_cown<Account> dst) { dst->frozen = true; };

      AtomicTransfer::transfer(src, dst, 50);

      when(src, dst) << [](acquired_cown<Account> src, acquired_cown<Account> dst) {
        check(src->balance == 100);
        check(dst->balance == 0);
      };
    }
  }

  namespace OrderingLogging {
    using Log = std::stringstream;

    void run() {
      cown_ptr<Account> src = make_cown<Account>(0);
      cown_ptr<Account> dst = make_cown<Account>(0);
      cown_ptr<Log> log = make_cown<Log>();

      when(log) << [](acquired_cown<Log> log) { *log << "begin" << std::endl; };

      when(src) << [log](acquired_cown<Account> src) {
        UNUSED(src);
        when(log) << [](acquired_cown<Log> log) { *log << "deposit" << std::endl; };
      };

      when(dst) << [log](acquired_cown<Account> dst) {
        UNUSED(dst);
        when(log) << [](acquired_cown<Log> log) { *log << "freeze" << std::endl; };
      };

      when(src, dst) << [log](acquired_cown<Account> src, acquired_cown<Account> dst) {
        UNUSED(src); UNUSED(dst);
        when(log) << [](acquired_cown<Log> log) { *log << "transfer" << std::endl; };
      };

      when(src, dst) << [log](acquired_cown<Account> src, acquired_cown<Account> dst) {
        UNUSED(src); UNUSED(dst);
        when(log) << [](acquired_cown<Log> log) {
          std::string s1, s2;

          *log >> s1;
          check(s1 == "begin");

          *log >> s1; *log >> s2;
          check((s1 == "deposit" && s2 == "freeze") || (s1 == "freeze" && s2 == "deposit"));

          *log >> s1;
          check(s1 == "transfer")
        };
      };
    }
  }

  void run() {
    AtomicTransfer::run();
    OrderingOperations::run();
    OrderingLogging::run();
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(Bank::run);
}
