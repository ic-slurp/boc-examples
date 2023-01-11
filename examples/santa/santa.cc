// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>

using namespace verona::cpp;

namespace SantaProblem {
  /*
   * The santa problem is presented as follows:
   * - there are 9 reindeer, 10 elves and 1 santa
   * - when santa and 9 reindeer are ready, they deliver presents and then the reindeer leave to go on holiday
   * - when santa and 3 elves are ready, they work together on R&D and then the elves go off to work
   * - if reindeer and elves are ready at the same time, then the reindeer get preference
   *
   * In this solution we have:
   * - a cown of santa
   * - a cown of a queue of available reindeers and likewise for elves
   * - a cown of a queue of a collection of 9 reindeer and likewise for 3 elves
   *
   * - Adding a reindeer to the reindeer pool will trigger a check to see if there are enough available reindeer to form a group:
   *   - if there are then 9 reindeer are removed from the pool and a placed into a new group
   *   - a behaviour is spawned to add the group to the ready reindeer queue, and subsequently
   *   - another behaviour is spawned (via process) to create a meeting of santa and elves or reindeer
   *   - this happens similarly for elves
   *
   * - Processing a change in state spawns a behaviour which acquires access to santa and both ready queues
   *   - if there are waiting reindeer then these are handled
   *   - otherwise there must be waiting elves which are handled
   *   - as process behaviours are only spawned as a result of a new group of ready entities being formed, there can
   *     never be process behaviours which cannot do anything
   *   - once complete, the elves or reindeer are returned to their original pools
   *
   * - We use Santa as a counter of how many meetings can occur so that the problem terminates
   */

  struct Santa { int count; }; // We use santa as a job count
  struct Reindeer {};
  struct Elf {};

  template<typename T>
  using Imm = std::shared_ptr<const T>;

  template<typename T>
  using Pool = std::queue<std::unique_ptr<T>>;

  template<typename T>
  using Group = std::vector<std::unique_ptr<T>>;

  template<typename T>
  using ReadyQueue = std::queue<Group<T>>;

  template<typename T>
  struct Collections {
    cown_ptr<Pool<T>> pool;
    cown_ptr<ReadyQueue<T>> ready;
    const size_t threshold;

    Collections(size_t threshold): pool(make_cown<Pool<T>>()), ready(make_cown<ReadyQueue<T>>()), threshold(threshold) {}
  };

  struct Workshop {
    cown_ptr<Santa> santa;
    Imm<Collections<Reindeer>> reindeer_collections;
    Imm<Collections<Elf>> elf_collections;

    template<typename T>
    static void add_entity(Imm<Workshop> ws, Imm<Collections<T>> collections, std::unique_ptr<T> entity) {
      when(collections->pool) << [ws, collections, entity = move(entity)](acquired_cown<Pool<T>> pool) mutable {
        pool->push(move(entity));

        if (pool->size() >= collections->threshold) {

          Group<T> sg(collections->threshold);
          size_t i = 0;
          while (i++ < collections->threshold) {
            sg.push_back(move(pool->front()));
            pool->pop();
          }

          when(collections->ready) << [sg = move(sg), ws](acquired_cown<ReadyQueue<T>> ready) mutable {
            ready->push(move(sg));
          };

          Workshop::process(ws);
        }
      };
    }

    template<typename T>
    static void return_entities(Imm<Workshop> ws, Imm<Collections<T>> collections, Group<T> used) {
      while (!used.empty()) {
        Workshop::add_entity(ws, collections, move(used.back()));
        used.pop_back();
      }
    }

    static void process(Imm<Workshop> ws) {
      when(ws->santa, ws->reindeer_collections->ready, ws->elf_collections->ready) << [ws](acquired_cown<Santa> santa, acquired_cown<ReadyQueue<Reindeer>> ready_reindeer, acquired_cown<ReadyQueue<Elf>> ready_elves){
        if((santa->count)-- > 0) {
          if(!ready_reindeer->empty()) {
            std::cout << "Reindeer and Santa meet to work" << std::endl;
            Workshop::return_entities<Reindeer>(ws, ws->reindeer_collections, move(ready_reindeer->front()));
            ready_reindeer->pop();

          } else if (!ready_elves->empty()){
            std::cout << "Elves and Santa meet to work" << std::endl;
            Workshop::return_entities<Elf>(ws, ws->elf_collections, move(ready_elves->front()));
            ready_elves->pop();

          } else {
            check(false && "we should not having pending processes without work available");
          }
        }
      };
    }

    Workshop(): santa(make_cown<Santa>(Santa{50})),
                reindeer_collections(std::make_shared<const Collections<Reindeer>>(9)),
                elf_collections(std::make_shared<const Collections<Elf>>(3)) {}

    static void create() {
      Imm<Workshop> ws = std::make_shared<const Workshop>();

      for (size_t i = 0; i < 9; ++i) {
        Workshop::add_entity<Reindeer>(ws, ws->reindeer_collections, std::make_unique<Reindeer>());
      }

      for (size_t i = 0; i < 10; ++i) {
        Workshop::add_entity<Elf>(ws, ws->elf_collections, std::make_unique<Elf>());
      }
    }
  };

  void run() {
    verona::rt::schedule_lambda([](){
      Workshop::create();
    });
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(SantaProblem::run);
}