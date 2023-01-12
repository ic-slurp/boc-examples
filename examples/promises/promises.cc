// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT

#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>
#include <deque>
#include <optional>

using namespace verona::cpp;

namespace promises {

  template<typename T>
  class promise {

    struct internal {
      std::optional<const T> v;
      std::deque<std::function<void(const T&)>> q;
    };

    cown_ptr<internal> inner;

public:
    promise(): inner(make_cown<internal>()) {}

    void then(std::function<void(const T&)> f) {
      when(inner) << [f=std::move(f)](acquired_cown<internal> inner) mutable {
        if (inner->v) {
          f(inner->v.value());
        } else {
          inner->q.emplace_back(move(f));
        }
      };
    }

    void fulfill(const T v) {
      when(inner) << [v=std::move(v)](acquired_cown<internal> inner) mutable {
        if (!inner->v) {
          inner->v.emplace(std::move(v));
          while (!inner->q.empty()) {
            inner->q.front()(inner->v.value());
            inner->q.pop_front();
          }
        }
      };
    }

  };

  namespace {
  template<size_t Idx=0, typename ...Args1, typename ...Args2>
    void _join(promise<std::tuple<Args1...>> p, std::tuple<Args2...> r, std::tuple<promise<Args1>...> ps) {
      if constexpr (sizeof...(Args1) == sizeof...(Args2)) {
        // now we have collected all of the arguments
        p.fulfill(r);
      } else {
        std::get<Idx>(ps).then([p, r, ps](const auto& v) mutable {
          _join<Idx + 1>(p, std::tuple_cat(r, std::make_tuple(v)), move(ps));
        });
      }
    }
  }

  template<typename ...Args>
  promise<std::tuple<Args...>> join(promise<Args> ... ps) {
    promise<std::tuple<Args...>> p;
    _join(p, std::make_tuple(), std::make_tuple(ps...));
    return p;
  }

  void run1() {
    // How do i join on promises?
    // Create some API that joins promises into a single promise of arrays of values

    promise<int> p = promise<int>();

    cown_ptr<int> c1 = make_cown<int>(0);
    cown_ptr<int> c2 = make_cown<int>(32);

    when(c1) << [p](acquired_cown<int> c) mutable {
      p.then([](const int& v){
        std::cout << "Fulfilled with: " << v << std::endl;
      });
    };

    when(c2) << [p](acquired_cown<int> c) mutable {
      p.fulfill(*c);
    };
  }

  void run2() {
    // How do i join on promises?
    // Create some API that joins promises into a single promise of arrays of values

    promise<int> p1 = promise<int>();
    promise<int> p2 = promise<int>();

    join(p1, p2).then([](std::tuple<int, int> ps){
      // for(const auto& v : ps) {
      std::cout << "Fulfilled with: " << std::get<0>(ps) << std::endl;
      std::cout << "Fulfilled with: " << std::get<1>(ps) << std::endl;
      // }
    });

    p1.fulfill(10);
    p2.fulfill(20);
  }

  class Foo {
  public:
    Foo(int) {}
  };

  void run3() {
    // How do i join on promises?
    // Create some API that joins promises into a single promise of arrays of values

    // promise<Foo> p1 = promise<Foo>();
    // promise<Foo> p2 = promise<Foo>();

    // join(p1, p2).then([](std::tuple<Foo, Foo> ps){
    //   // for(const auto& v : ps) {

    //   // }
    // });
  }

};



int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(promises::run2);
  return 0;
}
