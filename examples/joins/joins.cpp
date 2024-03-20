// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>

#include <optional>
#include <iostream>

using namespace verona::cpp;

using namespace std;

namespace Joins {

  struct Observer {
    virtual void notify() = 0;

    virtual ~Observer() {};
  };

  template<typename T>
  struct Channel {
    /* Channel has:
        - a queue of data to be read
        - a list of observers to notify whenever there is data
       Observers subscribe to the channel and whenever
       there is data they will be notified
    */
    queue<unique_ptr<T>> data;
    // polymorphic cown_ptr
    vector<cown_ptr<unique_ptr<Observer>>> observers;

    /*
      capture the cown in the notify so that the message holds a strong
      reference to the channel and the channel is not deallocated
      before an observer can when on the channel and observe the state
    */
    static void notify_all(acquired_cown<Channel<T>>& channel) {
      for (auto& observer : channel->observers) {
        when(observer) << [c=channel.cown()] (acquired_cown<unique_ptr<Observer>> observer) {
          assert(c);
          (*observer)->notify();
        };
      }
    }

    static void write(acquired_cown<Channel<T>>& channel, unique_ptr<T> value) {
      channel->data.push(move(value));
      Channel<T>::notify_all(channel);
    }

    static void write(cown_ptr<Channel<T>> channel, unique_ptr<T> value) {
      when(channel) << [value=move(value)] (acquired_cown<Channel<T>> channel) mutable {
        Channel<T>::write(channel, move(value));
      };
    }

    static unique_ptr<T> read(acquired_cown<Channel<T>>& channel) {
      if (channel->data.size() > 0) {
        unique_ptr<T> front = move(channel->data.front());
        channel->data.pop();
        if (channel->data.size() > 0)
          Channel<T>::notify_all(channel);
        return front;
      }
      return nullptr;
    }

    bool has_data() {
      return data.size() != 0;
    }

    static void subscribe(acquired_cown<Channel<T>>& channel, cown_ptr<unique_ptr<Observer>> observer) {
      channel->observers.push_back(observer);
      if (channel->data.size() > 0) {
        when(move(observer)) << [c=channel.cown()] (acquired_cown<unique_ptr<Observer>> observer) {
          assert(c);
          (*observer)->notify();
        };
      }
    }
  };

  template <typename T>
  static unique_ptr<T> read(acquired_cown<Channel<T>>& channel) {
    return Channel<T>::read(channel);
  }

  template <typename T>
  static void write(acquired_cown<Channel<T>>& channel, unique_ptr<T> value) {
    Channel<T>::write(channel, move(value));
  }

  template <typename T>
  static void write(cown_ptr<Channel<T>> channel, unique_ptr<T> value) {
    Channel<T>::write(move(channel), move(value));
  }

  template <typename T>
  static void subscribe(acquired_cown<Channel<T>>& channel, cown_ptr<unique_ptr<Observer>> observer) {
    Channel<T>::subscribe(channel, move(observer));
  }

  template<typename S, typename R>
  struct Message {
    /*
      A Message is a pair of:
        - Data to place on a channel
        - A callback to reply to (this is like a synchronous join)
      Either of these can be empty

      Note: ideally we could subclass this and make the message
      types nicer but polymorphism in cown_ptr<> is fiddly
    */
    optional<unique_ptr<S>> data;

    using F = function<void(unique_ptr<R>)>;
    optional<F> reply;

    Message(unique_ptr<S> data): data(move(data)), reply(nullopt) {};
    Message(unique_ptr<S> data, F reply): data(move(data)), reply(forward<F>(reply)) {};
    Message(F reply): data(nullopt), reply(forward<F>(reply)) {};
  };

  /* Data and Reply type aliases for convience */
  template<typename S>
  using DataMessage = Message<S, nullopt_t>;

  template<typename R>
  using ReplyMessage = Message<nullopt_t, R>;

  /*
    Pattern<N> consists of N channels
    There two important functions for these structs:
      - And : This continues building a pattern with one more channel
      - Do  : Registers a pattern as an observer on the channels,
              this takes a callback to run when there is data on
              all the channels

    The internal Pattern is the observer. When notified
    the Pattern checks all channels to which it is subscribed
    and if there is data on all of them, reads the data and
    executes the callback.
  */
  template<typename ...Args>
  struct Pattern {

    tuple<cown_ptr<Channel<Args>>...> channels;

    Pattern(cown_ptr<Channel<Args>>... channels): channels(channels...) {}

    template<typename M>
    Pattern<Args..., M> And(cown_ptr<Channel<M>> channel) {
      return apply([channel=move(channel)](auto &&...args) mutable {
        return Pattern<Args..., M>(args..., move(channel));
      }, move(channels));
    }

    using F = function<void(unique_ptr<Args>...)>;

    struct P : public Observer {
      tuple<typename cown_ptr<Channel<Args>>::weak...> channels;
      F f;

      P(F f, cown_ptr<Channel<Args>>... channels): f(forward<F>(f)), channels(channels.get_weak()...) {}
    
      void notify() {
        /* promote all channels to strong refs */
        tuple<cown_ptr<Channel<Args>>...> cs = apply([](auto &&...args) mutable {
          return make_tuple<cown_ptr<Channel<Args>>...>(move(args.promote())...);
        }, channels);

        /* If any cown can't be promoted it is because the channel has been deallocated
           thus the pattern can no longer match */
        if(apply([](auto &&...args) mutable { return (!args || ...) ; }, cs))
          return;

        /* Otherwise, attempt to read a value from all channels and call the pattern callback */
        apply([f=f](auto &&... args) mutable {
          when(args...) << [f=forward<F>(f)](acquired_cown<Channel<Args>>... channels) mutable {
            if ((!channels->has_data() || ...))
              return;
            f(read(channels)...);
          };
        }, move(cs));
      }
    };

    void Do(F run) {
      auto pattern = make_cown<unique_ptr<Observer>>(
        apply([run=forward<F>(run)](auto &&...args) mutable {
          return make_unique<P>(forward<F>(run), args...);
        }, channels));

        apply([pattern=move(pattern)](auto &&...args) mutable {
          when(args...) << [pattern=move(pattern)](acquired_cown<Channel<Args>>... channels) mutable {
            (subscribe(channels, pattern), ...);
          };
        }, move(channels));
    }
  };

  /*
    Join starts of the construction of a pattern
  */
  namespace Join {
    template<typename M>
    static Pattern<M> When(cown_ptr<Channel<M>> channel) {
      return Pattern<M>(channel);
    }
  };

  void run() {
    /*
      Builds a put and get channel:
        - put channel contains messages with data
        - get channel contains messages with replies
    */
    auto put = make_cown<Channel<DataMessage<int>>>();
    auto get = make_cown<Channel<ReplyMessage<int>>>();
    auto put_string = make_cown<Channel<DataMessage<string>>>();

    write(put, make_unique<DataMessage<int>>(make_unique<int>(20)));
    
    write(put, make_unique<DataMessage<int>>(make_unique<int>(51)));


    write(put_string, make_unique<DataMessage<string>>(make_unique<string>("a string ")));

    /* send a repliable message on get */
    write(get, make_unique<ReplyMessage<int>>([](unique_ptr<int> msg) mutable {
        cout << *msg << " -- 1" << endl;
    }));

    /* create a pattern so that if there is a message on put and get then reply to get */
    Join::When(put).And(get).And(put_string).Do([](unique_ptr<DataMessage<int>> put, unique_ptr<ReplyMessage<int>> get, unique_ptr<DataMessage<string>> put_string) {
      cout << **(put_string->data) << endl;
      (*(get->reply))(move(*(put->data)));
    });


    /* create a pattern so if there is a message on put then print it */
    Join::When(put).Do([](unique_ptr<DataMessage<int>> msg) {
      cout << **(msg->data) << " -- 0" << endl;
    });

    /* create a pattern so that if there is a message on put and get then reply to get */
    Join::When(put).And(get).Do([](unique_ptr<DataMessage<int>> put, unique_ptr<ReplyMessage<int>> get) {
      (*(get->reply))(move(*(put->data)));
    });

    write(get, make_unique<ReplyMessage<int>>([](unique_ptr<int> msg) mutable {
        cout << *msg << " -- 2" << endl;
    }));

    write(put, make_unique<DataMessage<int>>(make_unique<int>(409)));

    write(put_string, make_unique<DataMessage<string>>(make_unique<string>("a string ")));

  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(Joins::run);
}