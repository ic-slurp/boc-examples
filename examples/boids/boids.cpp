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

const size_t width = 800;
const size_t height = 600;
const int vlim = 10;

template<typename C>
static acquired_cown<C> cown_ptr_to_acquired(cown_ptr<C> c)
{
  return acquired_cown<C>(c);
}

template<size_t n, std::size_t... I>
void compute_partial_results_impl(std::vector<cown_ptr<Result>>& results,  std::array<cown_ptr<Boid>, n>& boids, std::index_sequence<I...>) {
  for (size_t i = 0; i < n; ++i) {
    cown_ptr<Result> partial_result = make_cown<Result>(Vector{0, 0}, Vector{0, 0}, Vector{0, 0});
    results.push_back(partial_result);

    when(partial_result, std::get<I>(boids)...) << [i](acquired_cown<Result> partial_result, decltype(cown_ptr_to_acquired(std::get<I>(boids)))... acquired){
      std::array<acquired_cown<Boid>*, n> boids {{ (&acquired)... }};
      for (size_t j = 0; j < n; ++j) {
        if (i != j) {
          std::get<0>(*partial_result) += (*boids[j])->position;
          Vector diff = (*boids[j])->position - (*boids[i])->position;
          if (diff.abs() < 30) std::get<1>(*partial_result) -= (diff / 2);;
          std::get<2>(*partial_result) += (*boids[j])->velocity;
        }
      }
    };
  }
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>>
void compute_partial_results(std::vector<cown_ptr<Result>>& results, std::array<cown_ptr<Boid>, n>& boids) {
  compute_partial_results_impl(results, boids, Indices{});
}

template<size_t n>
void update_boid_positions(std::vector<cown_ptr<Result>>& results, std::array<cown_ptr<Boid>, n>& boids) {
  for (size_t i = 0; i < n; ++i) {
    when(results[i], boids[i]) << [i](acquired_cown<Result> partial_result, acquired_cown<Boid> boid){
      std::get<0>(*partial_result) /= (n - 1);
      std::get<0>(*partial_result) = (std::get<0>(*partial_result) - boid->position) / 50;

      std::get<2>(*partial_result) /= (n - 1);
      std::get<2>(*partial_result) = (std::get<2>(*partial_result) - boid->velocity) / 16;

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
      v += std::get<0>(*partial_result) + std::get<1>(*partial_result) + std::get<2>(*partial_result) + v4;
      if (v.abs() > vlim) {
        boid->velocity = (v / v.abs()) * vlim;
      }
      boid->position += v;
    };
  }
}

void draw_boid(acquired_cown<sf::RenderWindow>& window, acquired_cown<Boid>& boid) {
  sf::CircleShape shape(5.f, 3);
  shape.setFillColor(sf::Color(0, 0, 0, 0));
  shape.setOutlineThickness(1.f);
  shape.setOutlineColor(sf::Color::Green);
  shape.setPosition(boid->position.x, boid->position.y);
  double r = atan2(boid->velocity.y, boid->velocity.x);
  shape.setRotation(r * (180 / M_PI));
  window->draw(shape);
}

template<size_t n, std::size_t... I>
void step_impl(cown_ptr<sf::RenderWindow> window, std::array<cown_ptr<Boid>, n> boids, std::index_sequence<I...>) {
  when() << [window, boids]() mutable {
    std::vector<cown_ptr<Result>> results;
    compute_partial_results(results, boids);
    update_boid_positions(results, boids);

    when(window, std::get<I>(boids)...) << [](acquired_cown<sf::RenderWindow> window, decltype(cown_ptr_to_acquired(std::get<I>(boids)))... acquired){
      window->clear();
      (draw_boid(window, acquired), ...);
      window->display();
    };

    step(window, boids);
  };
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>>
void step(cown_ptr<sf::RenderWindow> window, std::array<cown_ptr<Boid>, n> boids) {
  step_impl(window, boids, Indices{});
}

template<size_t n, std::size_t... I>
void run_impl(std::index_sequence<I...> is) {
  std::default_random_engine gen;
  std::uniform_int_distribution<int> x_dist(0, width - 1);
  std::uniform_int_distribution<int> y_dist(0, height - 1);

  // static cast inside tuple to unpack the parameters
  std::array<cown_ptr<Boid>, n> boids = {{ (static_cast<void>(I), make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}))... }};
  auto window = make_cown<sf::RenderWindow>(sf::VideoMode(width, height), "Boids");
  step(window, boids);
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>>
void run() {
  run_impl<n>(Indices{});
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(run<30>);
}