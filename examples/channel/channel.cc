// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <test/harness.h>
#include <cpp/when.h>

namespace Channels{
  /*
   * A channel is constructed from two queue:
   * - a queue of callbacks that are waiting for values
   * - a queue of values that are waiting to be used
   * - only one of these queues should ever be non-empty
   * - the static methods required a cown of a channel to be provided so the channel is only
   *   ever in use by one behaviour at a time
   * - behaviours are then scheduled to read from or write values to the channel
   */

  template<typename T>
  struct Channel {
    using F = std::function<void(std::unique_ptr<T>)>;
    std::queue<F> reads;
    std::queue<std::unique_ptr<T>> writes;

    static void write(cown_ptr<Channel<T>> channel, std::unique_ptr<T> value) {
      when(channel) << [value = std::move(value)](acquired_cown<Channel<T>> channel) mutable {
        if (channel->reads.size() > 0) {
          check(channel->writes.size() == 0);
          channel->reads.front()(std::move(value));
          channel->reads.pop();
        } else {
          channel->writes.push(std::move(value));
        }
      };
    }

    static void read(cown_ptr<Channel<T>> channel, F callback) {
      when(channel) << [callback](acquired_cown<Channel<T>> channel) mutable {
        if (channel->writes.size() > 0) {
          check(channel->reads.size() == 0);
          callback(std::move(channel->writes.front()));
          channel->writes.pop();
        } else {
          channel->reads.push(callback);
        }
      };
    }
  };

  void run() {
    cown_ptr<Channel<int>> channel = make_cown<Channel<int>>();

    when() << [channel](){
      Channel<int>::write(channel, std::make_unique<int>(2));
    };

    when() << [channel](){
      Channel<int>::read(channel, [](std::unique_ptr<int> value){
        std::cout << *value << std::endl;
      });
    };

    when() << [channel](){
      Channel<int>::read(channel, [](std::unique_ptr<int> value){
        std::cout << *value << std::endl;
      });
    };

    when() << [channel](){
      Channel<int>::write(channel, std::make_unique<int>(42));
    };
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(Channels::run);
}