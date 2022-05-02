// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace SantaProblem {
  struct Santa {
    int count;
    Santa(int count): count(count) {};
  };
  struct Reindeer {};
  struct Elf {};

  template<typename T>
  using Imm = std::shared_ptr<const T>;

  template<typename T>
  using Group = std::queue<std::unique_ptr<T>>;

  template<typename T>
  using WaitingGroups = std::queue<Group<T>>;

  template<typename T>
  struct EntityInfo {
    cown_ptr<Group<T>> group;
    cown_ptr<WaitingGroups<T>> waiting;
    size_t threshold;

    EntityInfo(size_t threshold): group(make_cown<Group<T>>()), waiting(make_cown<WaitingGroups<T>>()), threshold(threshold) {}
  };

  struct Workshop {
    cown_ptr<Santa> santa; // santa is just a job count here
    Imm<EntityInfo<Reindeer>> reindeer_info;
    Imm<EntityInfo<Elf>> elf_info;

    template<typename T>
    static void add_entity(Imm<Workshop> ws, Imm<EntityInfo<T>> info, std::unique_ptr<T> entity) {
      when(info->group) << [ws, info, entity = move(entity)](acquired_cown<Group<T>> group) mutable {
        group->push(move(entity));
        if (group->size() >= info->threshold) {
          Group<T> sg;
          size_t i = 0;
          while (i++ < info->threshold) {
            sg.push(move(group->front()));
            group->pop();
          }
          when(info->waiting) << [sg = move(sg), ws](acquired_cown<WaitingGroups<T>> waiting) mutable {
            waiting->push(move(sg));
            Workshop::process(ws);
          };
        }
      };
    }

    template<typename T>
    static void return_entities(Imm<Workshop> ws, Imm<EntityInfo<T>> info, Group<T> entities) {
      while (!entities.empty()) {
        Workshop::add_entity(ws, info, move(entities.front()));
        entities.pop();
      }
    }

    static void process(Imm<Workshop> ws) {
      when(ws->santa, ws->reindeer_info->waiting, ws->elf_info->waiting) << [ws](acquired_cown<Santa> santa, acquired_cown<WaitingGroups<Reindeer>> waiting_reindeer, acquired_cown<WaitingGroups<Elf>> waiting_elves){
        if((santa->count)-- > 0) {
          if(!waiting_reindeer->empty()) {
            std::cout << "Reindeer and Santa meet to work" << std::endl;
            Workshop::return_entities<Reindeer>(ws, ws->reindeer_info, move(waiting_reindeer->front()));
            waiting_reindeer->pop();
          } else if (!waiting_elves->empty()){
            std::cout << "Elves and Santa meet to work" << std::endl;
            Workshop::return_entities<Elf>(ws, ws->elf_info, move(waiting_elves->front()));
            waiting_elves->pop();
          } else {
            check(false && "we should not having pending processes without work available");
          }
        }
      };
    }

    Workshop(): santa(make_cown<Santa>(50)),
                reindeer_info(std::make_shared<const EntityInfo<Reindeer>>(9)),
                elf_info(std::make_shared<const EntityInfo<Elf>>(3)) {}

    static void create() {
      Imm<Workshop> ws = std::make_shared<const Workshop>();

      for (size_t i = 0; i < 9; ++i) {
        Workshop::add_entity<Reindeer>(ws, ws->reindeer_info, std::make_unique<Reindeer>());
      }

      for (size_t i = 0; i < 10; ++i) {
        Workshop::add_entity<Elf>(ws, ws->elf_info, std::make_unique<Elf>());
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