// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT

#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>
#include <deque>
#include <optional>
#include <variant>

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

    promise<T>& then(std::function<void(const T&)> f) {
      when(inner) << [f=std::move(f)](acquired_cown<internal> inner) mutable {
        if (inner->v) {
          f(inner->v.value());
        } else {
          inner->q.emplace_back(move(f));
        }
      };
      return *this;
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
    template<typename ...Args>
    void _join(promise<std::tuple<Args...>> p, std::tuple<Args...> r) {
      p.fulfill(r);
    }

    template<typename ...Args1, typename ...Args2, typename Arg, typename ...Args3>
    void _join(promise<std::tuple<Args1...>> p, std::tuple<Args2...> r, promise<Arg> pr, promise<Args3>... prs) {
      // Args1 = Args2 ++ [Arg] ++ Args3
      pr.then([p, r, prs...](const auto& v) mutable {
        _join(p, std::tuple_cat(r, std::make_tuple(v)), prs...);
      });
    }
  }

  template<typename ...Args>
  promise<std::tuple<Args...>> join(promise<Args>... ps) {
    promise<std::tuple<Args...>> p;
    _join(p, std::make_tuple(), ps...);
    return p;
  }

  template<typename ...Args>
  promise<std::variant<Args...>> any(promise<Args>... ps) {
    promise<std::variant<Args...>> p;
    (ps.then([p](const auto& v) mutable {p.fulfill(v);}),...);
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
      }).then([](const int& v){
        std::cout << "It didn't change" << std::endl;
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
    promise<char> p2 = promise<char>();

    join(p1, p2).then([](std::tuple<int, char> ps){
      // for(const auto& v : ps) {
      std::cout << "Fulfilled with: " << std::get<0>(ps) << std::endl;
      std::cout << "Fulfilled with: " << std::get<1>(ps) << std::endl;
      // }
    });

    p1.fulfill(10);
    p2.fulfill('b');
  }

  template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
  template<class... Ts> overload(Ts...) -> overload<Ts...>; // line not needed in C++20...

  void run3() {
    // How do i join on promises?
    // Create some API that joins promises into a single promise of arrays of values

    promise<int> p1 = promise<int>();
    promise<char> p2 = promise<char>();

    any(p1, p2).then([](std::variant<int, char> p){
      std::visit(overload{
        [](int& i)       { std::cout << "Got int " << i << std::endl; },
        [](char& c)   { std::cout << "Got char " << c << std::endl; }
      }, p);
    });

    p1.fulfill(10);
    p2.fulfill('b');
  }

  class Foo {
  public:
    Foo(int) {}
  };

  void run4() {
    // How do i join on promises?
    // Create some API that joins promises into a single promise of arrays of values

    promise<Foo> p1 = promise<Foo>();
    promise<Foo> p2 = promise<Foo>();

    join(p1, p2).then([](std::tuple<Foo, Foo> ps){
      // for(const auto& v : ps) {

    //   // }
    });
  }

};



int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(promises::run1);
  return 0;
}
