// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>
#include <cmath>
#include <random>
#include <SFML/Graphics.hpp>

struct Vector {
  double x;
  double y;

  void operator+=(Vector other) {
    x += other.x;
    y += other.y;
  }

  Vector operator+(const Vector other) const {
    Vector v{x, y};
    v += other;
    return v;
  }

  void operator/=(double scalar) {
    x /= scalar;
    y /= scalar;
  }

  Vector operator/(const double scalar) {
    Vector v{x, y};
    v /= scalar;
    return v;
  }

  void operator*=(double scalar) {
    x *= scalar;
    y *= scalar;
  }

  Vector operator*(const double scalar) const {
    Vector v{x, y};
    v *= scalar;
    return v;
  }

  void operator-=(Vector other){
    x -= other.x;
    y -= other.y;
  }

  Vector operator-(const Vector other) const {
    Vector v{x, y};
    v -= other;
    return v;
  }

  double abs() {
    return std::sqrt(std::pow(x, 2) + std::pow(y, 2));
  }

  friend std::ostream& operator<<(std::ostream&, const Vector&);
};

std::ostream& operator<<(std::ostream& os, const Vector& v)
{
  os << "{" << v.x << ", " << v.y << "}";
  return os;
}

struct Boid {
  Vector position;
  Vector velocity;

  Boid(Vector position) : position(position), velocity(Vector{0, 0}) {}

  friend std::ostream& operator<<(std::ostream&, const Boid&);
};

std::ostream& operator<<(std::ostream& os, const Boid& b)
{
  os << "position: " << b.position << " velocity: " << b.velocity;
  return os;
}

using Result = std::tuple<Vector, Vector, Vector>;

void old_step(const size_t width, const size_t height, cown_ptr<sf::RenderWindow> window, std::vector<cown_ptr<Boid>> boids, const size_t vlim) {
  when() << [width, height, window, boids = std::move(boids), vlim]() mutable {
    const size_t num_boids = boids.size();

    std::vector<cown_ptr<Result>> results;

    for (size_t i = 0; i < num_boids; ++i)
    {
      cown_ptr<Result> partial_result = make_cown<Result>(Vector{0, 0}, Vector{0, 0}, Vector{0, 0});
      results.push_back(partial_result);
      for (size_t j = 0; j < num_boids; ++j)
      {
        if (i != j) {
          when(boids[i], boids[j], partial_result) << [] (acquired_cown< Boid> boid, acquired_cown< Boid> other, acquired_cown<Result> result){
            std::get<0>(*result) += other->position;
            Vector diff = other->position - boid->position;
            if (diff.abs() < 30) std::get<1>(*result) -= (diff / 2);;
            std::get<2>(*result) += other->velocity;
          };
        }
      }
    }

    for (size_t i = 0; i < num_boids; ++i)
    {
      when(boids[i], results[i]) << [num_boids, width, height, vlim](acquired_cown<Boid> boid, acquired_cown<Result> result){
        std::get<0>(*result) /= (num_boids - 1);
        std::get<0>(*result) = (std::get<0>(*result) - boid->position) / 50;

        std::get<2>(*result) /= (num_boids - 1);
        std::get<2>(*result) = (std::get<2>(*result) - boid->velocity) / 16;

        Vector& p = boid->position;
        Vector v4{0, 0};

        if (p.x < 0) {
          v4.x = 10;
        } else if (p.x > width) {
          v4.x = -10;
        }

        if (p.y < 0) {
          v4.y = 10;
        } else if (p.y > height) {
          v4.y = -10;
        }

        Vector& v = boid->velocity;
        v += std::get<0>(*result) + std::get<1>(*result) + std::get<2>(*result) + v4;
        if (v.abs() > vlim) {
          boid->velocity = (v / v.abs()) * vlim;
        }
        boid->position += v;
      };
    }

    // this is a bit eager and will clear the window
    // can we do a when<N> template?
    // when(window) << [](acquired_cown<sf::RenderWindow> window) { window->clear(); };
    for (size_t i = 0; i < num_boids; ++i)
    {
      when(read(boids[i]), window) << [i](acquired_cown<Boid const> boid, acquired_cown<sf::RenderWindow> window) {
        std::cout << "boids[" << i << "]: " << *boid << std::endl;
        sf::CircleShape shape(5.f, 3);
        shape.setFillColor(sf::Color(0, 0, 0, 0));
        shape.setOutlineThickness(1.f);
        shape.setOutlineColor(sf::Color::Green);
        shape.setPosition(boid->position.x, boid->position.y);
        double r = atan2(boid->velocity.y, boid->velocity.x);
        shape.setRotation(r * (180 / M_PI));
        window->draw(shape);
      };
    }
    when(window) << [](acquired_cown<sf::RenderWindow> window) { window->display(); };

    old_step(width, height, window, std::move(boids), vlim);
  };
}

void old_ran() {
  const size_t width = 800;
  const size_t height = 600;
  cown_ptr<sf::RenderWindow> window = make_cown<sf::RenderWindow>(sf::VideoMode(width, height), "Boids");
  const size_t num_boids = 50;
  std::vector<cown_ptr<Boid>> boids;

  std::default_random_engine gen;
  std::uniform_int_distribution<int> x_dist(0, width - 1);
  std::uniform_int_distribution<int> y_dist(0, height - 1);
  for (size_t i = 0; i < num_boids; ++i) {
    boids.push_back(make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}));
  }

  const int vlim = 10;

  old_step(width, height, window, std::move(boids), vlim);
}

template<typename C>
static acquired_cown<C> cown_ptr_to_acquired(cown_ptr<C> c)
{
  return acquired_cown<C>(c);
}

template<size_t i, size_t j, typename... Ts>
void compute_partial_result(acquired_cown<Result>& partial_result, std::tuple<Ts...>& boids) {
  if constexpr (j >= sizeof...(Ts)) {
    return;
  } else if constexpr (i != j) {
      std::get<0>(*partial_result) += std::get<j>(boids)->position;
      Vector diff = std::get<j>(boids)->position - std::get<i>(boids)->position;
      if (diff.abs() < 30) std::get<1>(*partial_result) -= (diff / 2);;
      std::get<2>(*partial_result) += std::get<j>(boids)->velocity;
  } else {
    compute_partial_result<i, j+1>(partial_result, boids);
  }
}

template<size_t i, typename... Ts>
void compute_partial_results(std::vector<cown_ptr<Result>>& results, Ts... args) {
  if constexpr (i >= sizeof...(Ts)) {
    return;
  } else {
    cown_ptr<Result> partial_result = make_cown<Result>(Vector{0, 0}, Vector{0, 0}, Vector{0, 0});
    results.push_back(partial_result);

    when(partial_result, args...) << [](acquired_cown<Result> partial_result, decltype(cown_ptr_to_acquired(args))... acquired) {
      const size_t num_boids = sizeof...(Ts);
      std::tuple<decltype(cown_ptr_to_acquired(args))&...> boids(acquired...);
      compute_partial_result<i, 0>(partial_result, boids);
    };

    compute_partial_results<i+1>(results, args...);
  }
}

template<size_t i, typename... Ts>
void update_boid_positions(std::vector<cown_ptr<Result>>& results, const size_t width, const size_t height, const size_t vlim, Ts... args) {
  if constexpr (i >= sizeof...(Ts)) {
    return;
  } else {
    std::tuple<Ts...> boids(args...);
    when(std::get<i>(boids), results[i]) << [width, height, vlim](acquired_cown<Boid> boid, acquired_cown<Result> result){
      const size_t num_boids = sizeof...(Ts);
      std::get<0>(*result) /= (num_boids - 1);
      std::get<0>(*result) = (std::get<0>(*result) - boid->position) / 50;

      std::get<2>(*result) /= (num_boids - 1);
      std::get<2>(*result) = (std::get<2>(*result) - boid->velocity) / 16;

      Vector& p = boid->position;
      Vector v4{0, 0};

      if (p.x < 0) {
        v4.x = 10;
      } else if (p.x > width) {
        v4.x = -10;
      }

      if (p.y < 0) {
        v4.y = 10;
      } else if (p.y > height) {
        v4.y = -10;
      }

      Vector& v = boid->velocity;
      v += std::get<0>(*result) + std::get<1>(*result) + std::get<2>(*result) + v4;
      if (v.abs() > vlim) {
        boid->velocity = (v / v.abs()) * vlim;
      }
      boid->position += v;
    };
    update_boid_positions<i + 1>(results, width, height, vlim, args...);
  }
}

template<size_t i, typename... Ts>
void draw_boids(acquired_cown<sf::RenderWindow>& window, std::tuple<Ts...>& boids) {
  if constexpr (i >= sizeof...(Ts)) {
    return;
  } else {
    //     std::cout << "boids[" << i << "]: " << *boid << std::endl;
    acquired_cown<Boid>& boid = std::get<i>(boids);
    sf::CircleShape shape(5.f, 3);
    shape.setFillColor(sf::Color(0, 0, 0, 0));
    shape.setOutlineThickness(1.f);
    shape.setOutlineColor(sf::Color::Green);
    shape.setPosition(boid->position.x, boid->position.y);
    double r = atan2(boid->velocity.y, boid->velocity.x);
    shape.setRotation(r * (180 / M_PI));
    window->draw(shape);
    draw_boids<i + 1>(window, boids);
  }
}

template<typename... Ts>
void step(const size_t width, const size_t height, cown_ptr<sf::RenderWindow> window, const size_t vlim, Ts... args) {
  when() << [width, height, window, vlim, args...]() mutable {
    const size_t num_boids = sizeof...(Ts);

    std::vector<cown_ptr<Result>> results;
    compute_partial_results<0>(results, args...);

    update_boid_positions<0>(results, width, height, vlim, args...);

    when(window, args...) << [](acquired_cown<sf::RenderWindow> window, decltype(cown_ptr_to_acquired(args))... acquired) {
      window->clear();
      std::tuple<decltype(cown_ptr_to_acquired(args))&...> boids(acquired...);
      draw_boids<0>(window, boids);
      window->display();
    };

    step(width, height, window, vlim, args...);
  };
}

void run() {
  const size_t width = 800;
  const size_t height = 600;
  cown_ptr<sf::RenderWindow> window = make_cown<sf::RenderWindow>(sf::VideoMode(width, height), "Boids");
  // const size_t num_boids = 5;
  // std::vector<cown_ptr<Boid>> boids;

  std::default_random_engine gen;
  std::uniform_int_distribution<int> x_dist(0, width - 1);
  std::uniform_int_distribution<int> y_dist(0, height - 1);
  // for (size_t i = 0; i < num_boids; ++i) {
    // boids.push_back(make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}));
  // }

  const int vlim = 10;

  step(width, height, window, vlim,
    make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}),
    make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}),
    make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}),
    make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}));
}

void something(const size_t width, const size_t height, cown_ptr<sf::RenderWindow> window, const size_t vlim) {
  cown_ptr<Boid> a = make_cown<Boid>(Vector{10, 10});
  cown_ptr<Boid> b = make_cown<Boid>(Vector{20, 20});

  step(width, height, window, vlim, a, b);
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(run);
}