// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace SantaProblem {
  struct Reindeer {};
  struct Elf {};

  template<typename T>
  using Group = std::vector<T>;

  template<typename T>
  using WaitingGroups = std::vector<Group<T>>;

  template<typename T>
  struct EntityInfo {
    cown_ptr<Group<std::unique_ptr<T>>> group;
    cown_ptr<WaitingGroups<std::unique_ptr<T>>> waiting;
    size_t threshold;

    EntityInfo(size_t threshold): group(make_cown<Group<std::unique_ptr<T>>>()), waiting(make_cown<WaitingGroups<std::unique_ptr<T>>>()), threshold(threshold) {}
  };

  struct Workshop {
    cown_ptr<int> santa; // santa is just a job count here

    std::shared_ptr<const EntityInfo<Reindeer>> reindeer_info;
    std::shared_ptr<const EntityInfo<Elf>> elf_info;

    template<typename T>
    static void add_entity(std::shared_ptr<const Workshop> ws, std::shared_ptr<const EntityInfo<T>> info, std::unique_ptr<T> entity) {
      when(info->group) << [ws, info, entity = std::move(entity)](acquired_cown<Group<std::unique_ptr<T>>> group) mutable {
        group->push_back(std::move(entity));
        if (group->size() >= info->threshold) {
          Group<std::unique_ptr<T>> sg;
          auto it = group->begin();
          size_t i = 0;
          while (i++ < info->threshold) {
            sg.push_back(std::move(*it));
            it = group->erase(it);
          }
          when(info->waiting) << [sg = std::move(sg), ws](acquired_cown<WaitingGroups<std::unique_ptr<T>>> waiting) mutable {
            waiting->push_back(std::move(sg));
            Workshop::process(ws);
          };
        }
      };
    }

    template<typename T>
    static void return_entities(std::shared_ptr<const Workshop> ws, std::shared_ptr<const EntityInfo<T>> info, Group<std::unique_ptr<T>> entities) {
      auto it = entities.begin();
      while (it != entities.end()) {
        Workshop::add_entity(ws, info, std::move(*it));
        it = entities.erase(it);
      }
    }

    static void process(std::shared_ptr<const Workshop> ws) {
      when(ws->santa, ws->reindeer_info->waiting, ws->elf_info->waiting) << [ws](acquired_cown<int> santa, acquired_cown<WaitingGroups<std::unique_ptr<Reindeer>>> waiting_reindeer, acquired_cown<WaitingGroups<std::unique_ptr<Elf>>> waiting_elves){
        if((*santa)-- > 0) {
          if(!waiting_reindeer->empty()) {
            auto reindeer = waiting_reindeer->begin();
            std::cout << "Reindeer and Santa meet to work" << std::endl;
            Workshop::return_entities<Reindeer>(ws, ws->reindeer_info, std::move(*reindeer));
            waiting_reindeer->erase(reindeer);
          } else if (!waiting_elves->empty()){
            auto elves = waiting_elves->begin();
            std::cout << "Elves and Santa meet to work" << std::endl;
            Workshop::return_entities<Elf>(ws, ws->elf_info, std::move(*elves));
            waiting_elves->erase(elves);
          } else {
            check(false && "we should not be able to get here");
          }
        }
      };
    }

    Workshop(): santa(make_cown<int>(int{50})),
                reindeer_info(std::make_shared<EntityInfo<Reindeer>>(9)),
                elf_info(std::make_shared<EntityInfo<Elf>>(3)) {}

    static void create() {
      const std::shared_ptr<const Workshop> ws = std::make_shared<const Workshop>();

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