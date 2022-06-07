// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace ReadOnly {

  struct Account {
    int balance;
    bool frozen;

    Account(int balance): balance(balance), frozen(false) {}
  };

  size_t num_accounts = 1 << 10;
  size_t work_usec = 10000;

  template<typename T>
  constexpr cown_ptr<T> write(cown_ptr<T> o) { return o; }

  template<typename To, typename From>
  using AccessOp = cown_ptr<To> (*)(cown_ptr<From>);

  template<typename To, typename From>
  void run(AccessOp<To, From> op) {
    // If we have infinite threads, we should be able to parallelise completely
    // the bulk of the work in the for loop.
    // This means the optimal execution we're looking for is ~ 5 * work_usec

    // The start and end jobs always add ~ 3 * work_usec
    // These are here to show happens before relations between write/read, read/write, read/read requests
    // The time will mostly be dominated by the for loop jobs.

    // For 'n' cores, we can execute 'n' parallel jobs at once
    // So add ~ ((2 * num_accounts) / n) * work_usec

    // For num_accounts = 1<<10, n = 4, work_usec=10000 this is:
    //   (2 * (1 << 10) / 4) * 10000 = (2048 / 4) * 10000 = 5120000 usec = 5.12 secs
    // Overall ~ 5.12 + 0.03 = 5.15 secs

    std::vector<cown_ptr<Account>> accounts;
    for (size_t i = 0 ; i < num_accounts; i++)
      accounts.push_back(make_cown<Account>(0));

    cown_ptr<Account> common_account = make_cown<Account>(100);
    when(common_account) << [](acquired_cown<Account> account) {
      busy_loop(work_usec);
      account->balance -= 10;
    };

    // 2 * num_accounts potentially parallel jobs
    for (size_t i = 0 ; i < num_accounts; i++)
    {
      when(accounts[i], op(common_account)) << [](acquired_cown<Account> write_account, acquired_cown<To> ro_account) {
        busy_loop(work_usec);
        write_account->balance = ro_account->balance;
      };

      when(op(accounts[i])) << [](acquired_cown<To> account) {
        busy_loop(work_usec);
        check(account->balance == 90);
      };
    }

    when(common_account) << [](acquired_cown<Account> account) {
      busy_loop(work_usec);
      account->balance += 10;
    };

    when(op(common_account)) << [](acquired_cown<To> account) {
      busy_loop(work_usec);
      check(account->balance == 100);
    };
  }

  void test_write() { run(&write<Account>); }

  void test_read() { run(&read<Account>); }

}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  if (harness.opt.has("--ro"))
    harness.run(ReadOnly::test_read);
  else
    harness.run(ReadOnly::test_write);
}