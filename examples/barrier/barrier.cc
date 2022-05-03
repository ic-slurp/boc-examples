// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace Test1 {
struct Participant
{
  int count;
  Participant(int count): count(count) {};
};

/*
 * Construct a barrier example using only the happens before ordering:
 *   - The behaviours decerementing p1 or p2 will complete before the first barrier
 *   - The behaviours incrementing p1 or p2 will complete after the first barrier but before the second
 */
void run()
{
  verona::rt::schedule_lambda([](){
    cown_ptr<Participant> p1 = make_cown<Participant>(10);
    cown_ptr<Participant> p2 = make_cown<Participant>(20);

    when(p1) << [](acquired_cown<Participant> p) mutable { p->count--; };

    when(p2) << [](acquired_cown<Participant> p) mutable { p->count--; };

    when(p1, p2) << [](acquired_cown<Participant> p1, acquired_cown<Participant> p2) mutable {
      check(p1->count == 9);
      check(p2->count == 19);
    };

    when(p1) << [](acquired_cown<Participant> p) mutable { p->count++; };

    when(p2) << [](acquired_cown<Participant> p) mutable { p->count++; };

    when(p1, p2) << [](acquired_cown<Participant> p1, acquired_cown<Participant> p2) mutable {
      check(p1->count == 10);
      check(p2->count == 20);
    };
  });
}
}

namespace Test2 {
  /*
 * Construct a barrier example using data flow:
 *   - A barrier structure is used to co-locate participants until all are ready to perform the next step
 *   - Participants are capture by behaviours, operated on and then added to a barrier structure
 */

struct Barrier;
struct Participant {
  size_t id;
  size_t count;
  Participant(size_t id): id(id), count((id + 1) * 10) {}
  static void begin(cown_ptr<Barrier>, std::unique_ptr<Participant>);
  static void mid(cown_ptr<Barrier>, std::unique_ptr<Participant>);
  static void end(cown_ptr<Barrier>, std::unique_ptr<Participant>);

  static void mid_check(std::unique_ptr<Participant> const&);
  static void end_check(std::unique_ptr<Participant> const&);
};

struct Barrier
{
  size_t count;
  const size_t reset;
  std::vector<std::unique_ptr<Participant>> participants;

  Barrier(size_t count): count(count), reset(count) {}

  using Next = std::function<void(cown_ptr<Barrier>, std::unique_ptr<Participant>)>;
  using Check = std::function<void(std::unique_ptr<Participant> const&)>;

  static void wait(cown_ptr<Barrier> barrier, std::unique_ptr<Participant> p, Next pnext, Check pcheck) {

    when(barrier) << [p = std::move(p), barrier, pnext, pcheck](acquired_cown<Barrier> b) mutable {
      b->participants.push_back(std::move(p));
      if (--b->count == 0) {
        for(const auto& p: b->participants)
          pcheck(p);

        while (!b->participants.empty()) {
          pnext(barrier, std::move(b->participants.back()));
          b->participants.pop_back();
        }

        b->count = b->reset;
      }
    };

  }
};

void Participant::begin(cown_ptr<Barrier> barrier, std::unique_ptr<Participant> p) {
  when() << [barrier, p = std::move(p)]() mutable {
    p->count--;
    Barrier::wait(barrier, std::move(p), Participant::mid, Participant::mid_check);
  };
}

void Participant::mid(cown_ptr<Barrier> barrier, std::unique_ptr<Participant> p) {
  when() << [barrier, p = std::move(p)]() mutable {
    p->count++;
    Barrier::wait(barrier, std::move(p), Participant::end, Participant::end_check);
  };
}

void Participant::end(cown_ptr<Barrier> barrier, std::unique_ptr<Participant> p) {
  UNUSED(barrier);
  UNUSED(p);
}

void Participant::mid_check(std::unique_ptr<Participant> const& p) {
  check(p->count  == (p->id + 1) * 10 - 1);
}

void Participant::end_check(std::unique_ptr<Participant> const& p) {
  check(p->count  == (p->id + 1) * 10);
}

void run()
{
  verona::rt::schedule_lambda([](){
    size_t size = 3;

    cown_ptr<Barrier> barrier = make_cown<Barrier>(size);

    for (size_t i = 0; i < size; ++i) {
      Participant::begin(barrier, std::make_unique<Participant>(i));
    }
  });
}
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(Test1::run);
  harness.run(Test2::run);
}
