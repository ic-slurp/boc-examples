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

  Vector operator+(Vector other) {
    Vector v{x, y};
    v += other;
    return v;
  }

  void operator/=(double scalar) {
    x /= scalar;
    y /= scalar;
  }

  Vector operator/(double scalar) {
    Vector v{x, y};
    v /= scalar;
    return v;
  }

  void operator*=(double scalar) {
    x *= scalar;
    y *= scalar;
  }

  Vector operator*(double scalar) {
    Vector v{x, y};
    v *= scalar;
    return v;
  }

  void operator-=(Vector other){
    x -= other.x;
    y -= other.y;
  }

  Vector operator-(Vector other){
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
};

struct World {
  std::vector<Boid> boids;
  size_t width;
  size_t height;

  World(size_t n, size_t width, size_t height) : width(width), height(height) {
    std::default_random_engine gen;
    std::uniform_int_distribution<int> x_dist(0, width - 1);
    std::uniform_int_distribution<int> y_dist(0, height - 1);
    for (; n > 0; n--) {
      boids.emplace_back(Vector{double(x_dist(gen)), double(y_dist(gen))});
    }
  }

  void draw_boids(sf::RenderWindow& window) {
    for (Boid& boid: boids) {
      sf::CircleShape shape(5.f, 3);
      shape.setFillColor(sf::Color(0, 0, 0, 0));
      shape.setOutlineThickness(1.f);
      shape.setOutlineColor(sf::Color::Green);
      shape.setPosition(boid.position.x, boid.position.y);
      double r = atan2(boid.velocity.y, boid.velocity.x);
      shape.setRotation(r * (180 / M_PI));
      window.draw(shape);
    }
  }

  Vector rule1(size_t idx) {
    Vector pcj{0, 0};

    for (size_t i = 0; i < boids.size(); ++i) {
      if (i != idx) {
        pcj += boids[i].position;
      }
    }

    pcj /= (boids.size() - 1);
    return (pcj - boids[idx].position) / 50;
  }

  Vector rule2(size_t idx) {
    Vector c{0, 0};

    for (size_t i = 0; i < boids.size(); ++i) {
      if (i != idx) {
        Vector diff = boids[i].position - boids[idx].position;
        if (diff.abs() < 30) {
          c -= (diff / 2);
        }
      }
    }

    return c;
  }

  Vector rule3(size_t idx) {
    Vector pvj{0, 0};

    for (size_t i = 0; i < boids.size(); ++i) {
      if (i != idx) {
        pvj += boids[i].velocity;
      }
    }

    pvj /= (boids.size() - 1);

    return (pvj - boids[idx].velocity) / 16;
  }

  Vector bound_position(size_t idx) {
    Boid& boid = boids[idx];
    Vector& v = boid.position;

    Vector r{0, 0};

    if (v.x < 0) {
      r.x = 10;
    } else if (v.x > width) {
      r.x = -10;
    }

    if (v.y < 0) {
      r.y = 10;
    } else if (v.y > height) {
      r.y = -10;
    }

    return r;
  }

  void limit_velocity(Boid& boid) {
    static const double vlim = 10;
    Vector& v = boid.velocity;
    std::cout << "v = " << v << " |v|= " << v.abs() << std::endl;
    if (v.abs() > vlim) {
      std::cout << "lim" << std::endl;
      std::cout << "(v / v.abs())" << (v / v.abs()) << std::endl;
      std::cout << "(v / v.abs()) * vlim" << (v / v.abs()) * vlim << std::endl;
      boid.velocity = (v / v.abs()) * vlim;
    }
  }

  void move_all_boids() {
    Vector v1, v2, v3, v4;
    for (size_t i = 0; i < boids.size(); ++i) {
      v1 = rule1(i);
      std::cout << "Boid " << i << " v1: {" << v1.x << ", " << v1.y << "}" << std::endl;
      v2 = rule2(i);
      std::cout << "Boid " << i << " v2: {" << v2.x << ", " << v2.y << "}" << std::endl;
      v3 = rule3(i);
      std::cout << "Boid " << i << " v3: {" << v3.x << ", " << v3.y << "}" << std::endl;
      v4 = bound_position(i);
      std::cout << "Boid " << i << " v4: " << v4 << std::endl;


      Boid& boid = boids[i];
      boid.velocity += v1 + v2 + v3 + v4;
      limit_velocity(boid);
      boid.position += boid.velocity;
      std::cout << "Boid " << i << ": " << boid.position << " " << boid.velocity << std::endl;
    }
    std::cout << "--------------" << std::endl;
  }
};

  // void run() {
  //   while(true) {
  //     draw_boids();
  //     move_all_boids();
  //   }
  // }

int main(int argc, char** argv)
{
    size_t width = 800;
  size_t height = 600;
  sf::RenderWindow window(sf::VideoMode(width, height), "Boids");

  World world(10, width, height);

  //  n boid cowns
  //  n resulting velocity cowns
  // while(true) {
  //  for result in results
  //    when(read(boids), result) { result = compute new boid velocity }
  //  for boid, result in zip(boids, results)
  //    when(boid, result) { boid.velocity = result; boid.position += boid.velocity }
  //  when(read(boids), window) { draw all boids }
  // }

  while (window.isOpen())
  {
      sf::Event event;
      while (window.pollEvent(event))
      {
          if (event.type == sf::Event::Closed)
              window.close();
      }

      window.clear();

      world.draw_boids(window);
      world.move_all_boids();

      window.display();
  }

  // SystematicTestHarness harness(argc, argv);
}