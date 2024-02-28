// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>
#include <cmath>
#include <random>
#include <SFML/Graphics.hpp>

/* Based on https://vergenet.net/~conrad/boids/pseudocode.html
 * with parameter tweaks and modifications to split boid
 * updates into two phases: global and local computations
 */

using namespace verona::cpp;

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

/* store the aggregated positions, velocity */
using Result = std::tuple<Vector, Vector, Vector>;

const size_t width = 800;
const size_t height = 600;
const int vlim = 20;

static size_t work_usec = 10000;

template<typename To, typename From>
using AccessOp = cown_ptr<To> (*)(cown_ptr<From>);

template<typename T>
constexpr cown_ptr<T> write(cown_ptr<T> o) { return o; }

template<typename T, size_t>
using acquired = acquired_cown<T>;

template<size_t n, std::size_t... I, typename To, typename From>
void compute_partial_results_impl(AccessOp<To, From> op, std::array<cown_ptr<Result>, n>& results,  std::array<cown_ptr<Boid>, n>& boids, std::index_sequence<I...>) {
  for (size_t i = 0; i < n; ++i) {
    when(results[i], op(std::get<I>(boids))...) << [i](acquired_cown<Result> partial_result, acquired<To, I>... acquired){
      std::array<acquired_cown<To>*, n> boids {{ (&acquired)... }};
      for (size_t j = 0; j < n; ++j) {
        if (i != j) {
          // Rule 1: collect sum of boid positions
          std::get<0>(*partial_result) += (*boids[j])->position;

          // Rule 2: If the boid is 'close' (here within 30) then
          // collect the (displacement * 2) to move boid away from these other boids
          Vector diff = (*boids[j])->position - (*boids[i])->position;
          if (diff.abs() < 30) std::get<1>(*partial_result) -= diff;

          // Rule 3: Collect the velocity of boids in the flock
          std::get<2>(*partial_result) += (*boids[j])->velocity;
        }
      }
    };
  }
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>, typename To, typename From>
void compute_partial_results(AccessOp<To, From> op, std::array<cown_ptr<Result>, n>& results, std::array<cown_ptr<Boid>, n>& boids) {
  compute_partial_results_impl(op, results, boids, Indices{});
}

template<size_t n>
void update_boid_positions(std::array<cown_ptr<Result>, n>& results, std::array<cown_ptr<Boid>, n>& boids) {
  for (size_t i = 0; i < n; ++i) {
    when(results[i], boids[i]) << [i](acquired_cown<Result> partial_result, acquired_cown<Boid> boid){
      // This behaviour calculates the velocity update for a particular
      // boid based on the global information

      // Rule 1:
      // calculate 'perceived' centre of mass (don't include self)
      std::get<0>(*partial_result) /= (n - 1);
      // calculate the velocity required to move the boids 1/120 towards the centre of mass
      // (an arbitrary number that made the motion looks smooth)
      std::get<0>(*partial_result) = (std::get<0>(*partial_result) - boid->position) / 120;

      // Rule 2:
      // Nothing to do as it was collected in the compute_partial step

      // Rule 3:
      // calculate the perceived velocity and add a fraction of it (1/8) to the boids
      // velocity
      std::get<2>(*partial_result) /= (n - 1);
      std::get<2>(*partial_result) = (std::get<2>(*partial_result) - boid->velocity) / 8;

      // Rule 4
      // Try to return the boid to the center of the screen
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

      // compute the boids velocity and bound it within some upper limit if necessary
      Vector& v = boid->velocity;
      v += std::get<0>(*partial_result) + std::get<1>(*partial_result) + std::get<2>(*partial_result) + v4;
      if (v.abs() > vlim) {
        boid->velocity = (v / v.abs()) * vlim;
      }
      boid->position += v;
    };
  }
}

template<typename To>
void draw_boid(acquired_cown<sf::RenderWindow>& window, acquired_cown<To>& boid) {
  sf::CircleShape shape(5.f, 3);
  shape.setFillColor(sf::Color(0, 0, 0, 0));
  shape.setOutlineThickness(1.f);
  shape.setOutlineColor(sf::Color::Green);
  shape.setPosition(boid->position.x, boid->position.y);
  double r = atan2(boid->velocity.y, boid->velocity.x);
  shape.setRotation(r * (180 / M_PI));
  window->draw(shape);
}

template<size_t n, std::size_t... I, typename To, typename From>
void step_impl(AccessOp<To, From> op, cown_ptr<sf::RenderWindow> window, std::array<cown_ptr<Boid>, n> boids, std::index_sequence<I...>) {
  when() << [op, window, boids]() mutable {
    std::array<cown_ptr<Result>, n> partial_results =  {{ (static_cast<void>(I), make_cown<Result>(Vector{0, 0}, Vector{0, 0}, Vector{0, 0}))... }};
    compute_partial_results(op, partial_results, boids);
    update_boid_positions(partial_results, boids);

    when(window, op(std::get<I>(boids))...) << [](acquired_cown<sf::RenderWindow> window, acquired<To, I>... acquired){
      window->clear();
      (draw_boid(window, acquired), ...);
      window->display();
    };

    step(op, window, boids);
  };
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>, typename To, typename From>
void step(AccessOp<To, From> op, cown_ptr<sf::RenderWindow> window, std::array<cown_ptr<Boid>, n> boids) {
  step_impl(op, window, boids, Indices{});
}

template<size_t n, std::size_t... I, typename To, typename From>
void run_impl(AccessOp<To, From> op, std::index_sequence<I...> is) {
  std::default_random_engine gen;
  std::uniform_int_distribution<int> x_dist(0, width - 1);
  std::uniform_int_distribution<int> y_dist(0, height - 1);

  // static cast inside tuple to unpack the parameters
  std::array<cown_ptr<Boid>, n> boids = {{ (static_cast<void>(I), make_cown<Boid>(Vector{double(x_dist(gen)), double(y_dist(gen))}))... }};
  auto window = make_cown<sf::RenderWindow>(sf::VideoMode(width, height), "Boids");
  step(op, window, boids);
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>>
void run_read() {
  run_impl<n>(&read<Boid>, Indices{});
}

template<std::size_t n, typename Indices = std::make_index_sequence<n>>
void run_write() {
  run_impl<n>(&write<Boid>, Indices{});
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  constexpr size_t num_boids = 50;
  if (harness.opt.has("--ro"))
    harness.run(run_read<num_boids>);
  else
    harness.run(run_write<num_boids>);
}