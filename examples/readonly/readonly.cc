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
    std::vector<cown_ptr<Account>> accounts;
    for (size_t i = 0 ; i < num_accounts; i++)
      accounts.push_back(make_cown<Account>(0));

    cown_ptr<Account> common_account = make_cown<Account>(100);
    when(common_account) << [](acquired_cown<Account> account) { account->balance -= 10; };

    // num_accounts parallel jobs
    for (size_t i = 0 ; i < num_accounts; i++)
      when(accounts[i], op(common_account)) << [](acquired_cown<Account> write_account, acquired_cown<To> ro_account) {
        busy_loop(work_usec);
        write_account->balance = ro_account->balance;
      };

    // num_accounts jobs
    for (size_t i = 0 ; i < num_accounts; i++)
      when(op(accounts[i])) << [](acquired_cown<To> account) {
        busy_loop(work_usec);
        check(account->balance == 90);
      };

    when(common_account) << [](acquired_cown<Account> account) { account->balance += 10; };

    when(op(common_account)) << [](acquired_cown<To> account) {
      busy_loop(work_usec);
      check(account->balance == 100);
    };

    when(common_account) << [] (acquired_cown<Account> account) { std::cout << "finished " << std::endl; };

    // (2 * num_accounts) + 4 ~= (2 * num_accounts) behaviours
    // half of which can be done in parallel
    // optimal 2x speedup
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