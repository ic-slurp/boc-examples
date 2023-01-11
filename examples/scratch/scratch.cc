// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>

using namespace verona::cpp;

namespace ReaderWriterCowns
{
  /*
   * A collection of illustrative examples for:
   * - creating and accessing cowns
   * - spawning behaviours behaviours using 'when'
   *
   * These are illustrated in the context of accessing isolated bank accounts
   */

  struct Account {
    int balance;
    bool frozen;

    Account(int balance): balance(balance), frozen(false) {}
  };

  /*
    * - A cown with contents T is constructed with make_cown<T>.
    * - This either constructs a T in place or constructs a cown from a T value,
    *   returning a cown_ptr<T>.
    * - The cown_ptr<T> cannot be directly dereferenced to access the contents.
    */
  size_t num_accounts = 8;
  size_t work_usec = 100000;

  void run_with_ro() {
    std::vector<cown_ptr<Account>> accounts;
    for (size_t i = 0 ; i < num_accounts; i++)
      accounts.push_back(make_cown<Account>(0));

    cown_ptr<Account> common_account = make_cown<Account>(100);
    when(common_account) << [](acquired_cown<Account> account) { account->balance -= 10; };

    for (size_t i = 0 ; i < num_accounts; i++)
      when(accounts[i], read(common_account)) << [](acquired_cown<Account> write_account, acquired_cown<const Account> ro_account) {
        busy_loop(work_usec);
        write_account->balance = ro_account->balance;
      };

    for (size_t i = 0 ; i < num_accounts; i++)
      when(read(accounts[i])) << [](acquired_cown<const Account> account) {
        busy_loop(work_usec);
        check(account->balance == 90);
      };

    when(common_account) << [](acquired_cown<Account> account) { account->balance += 10; };

    when(read(common_account)) << [](acquired_cown<const Account> account) {
      busy_loop(work_usec);
      check(account->balance == 100);
    };
  }

  void run_without_ro() {
    std::vector<cown_ptr<Account>> accounts;
    for (size_t i = 0 ; i < num_accounts; i++)
      accounts.push_back(make_cown<Account>(0));

    cown_ptr<Account> common_account = make_cown<Account>(100);
    when(common_account) << [](acquired_cown<Account> account) { account->balance -= 10; };

    for (size_t i = 0 ; i < num_accounts; i++)
      when(accounts[i], common_account) << [](acquired_cown<Account> write_account, acquired_cown<Account> ro_account) {
        write_account->balance = ro_account->balance;
      };

    for (size_t i = 0 ; i < num_accounts; i++)
      when(accounts[i]) << [](acquired_cown<Account> account) {
        check(account->balance == 90);
      };

    when(common_account) << [](acquired_cown<Account> account) { account->balance += 10; };

    when(common_account) << [](acquired_cown<Account> account) {
      check(account->balance == 100);
    };
  }

  void run_with_everything_ro() {
    std::vector<cown_ptr<Account>> accounts;
    for (size_t i = 0 ; i < num_accounts; i++)
      accounts.push_back(make_cown<Account>(100));

    cown_ptr<Account> common_account = make_cown<Account>(100);

    for (size_t i = 0 ; i < num_accounts; i++)
      when(read(accounts[i])) << [](acquired_cown<const Account> account) {
        check(account->balance == 100);
      };

    for (size_t i = 0 ; i < num_accounts; i++)
      when(read(accounts[i]), read(common_account)) << [](acquired_cown<const Account> write_account, acquired_cown<const Account> ro_account) {
        check(write_account->balance == ro_account->balance);
      };

    when(read(common_account)) << [](acquired_cown<const Account> account) {
      check(account->balance == 100);
    };

    for (size_t i = 0 ; i < num_accounts; i++)
      when(accounts[i]) << [](acquired_cown<Account> account) {
        check(account->balance == 100);
      };
  }

  void run_with_ro_short() {
    cown_ptr<Account> common_account = make_cown<Account>(100);

    for (size_t i = 0 ; i < num_accounts; i++)
      when(read(common_account)) << [i](acquired_cown<const Account> ro_account) {
        std::cout << "start " << i+1 << std::endl;
        busy_loop(work_usec);
        check(ro_account->balance == 100);
        std::cout << "end " << i + 1 << std::endl;
      };

    when(common_account) << [](acquired_cown<Account> account) {
      std::cout << "complete" << std::endl;
    };
  }

  void hit_batch_limit() {
    cown_ptr<Account> common_account = make_cown<Account>(100);

    for (size_t i = 0 ; i < 200; i++)
      when(common_account) << [](acquired_cown<Account> ro_account) {
        check(ro_account->balance == 100);
      };
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(ReaderWriterCowns::run_with_ro_short);
  // harness.run(ReaderWriterCowns::run_without_ro);
}
