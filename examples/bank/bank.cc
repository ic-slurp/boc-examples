// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <test/log.h>
#include <cpp/when.h>

namespace Bank
{
  struct Account {
    int balance;
    bool frozen;

    Account(int balance): balance(balance), frozen(false) {}
  };

  namespace AccessViolation {
    void run() {
      cown_ptr<Account> account = make_cown<Account>(100);
      // account->amount -= 100; illegal access of cown state
    }
  }

  namespace SchedulingWork {
    void transfer(cown_ptr<Account> src, cown_ptr<Account> dst, int amount) {
      when(src) << [amount](acquired_cown<Account> src) { src->balance -= amount; };
      when(dst) << [amount](acquired_cown<Account> dst) { dst->balance += amount; };
    }
  }

  namespace NestingBehaviours {
    void transfer(cown_ptr<Account> src, cown_ptr<Account> dst, int amount) {
      when(src) << [dst, amount](acquired_cown<Account> src) {
        src->balance -= amount;
        when(dst) << [amount](acquired_cown<Account> dst) { dst->balance += amount; };
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
    using logger::Log;

    void run() {
      cown_ptr<Account> src = make_cown<Account>(0);
      cown_ptr<Account> dst = make_cown<Account>(0);
      cown_ptr<Log> log = make_cown<Log>(std::cout);

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
        UNUSED(src);
        UNUSED(dst);
        when(log) << [](acquired_cown<Log> log) { *log << "transfer" << std::endl; };
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
