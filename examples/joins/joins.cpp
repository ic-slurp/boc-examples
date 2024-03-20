// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>

#include <optional>
#include <iostream>

using namespace verona::cpp;

using namespace std;

/*
  Note: this example currently leaks memory on shutdown.
  There is a strong cown ref cycle between channels and patterns.
  If we switch either to weak then we lose messages or patterns on shutdown.
*/

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

    void notify_all() {
      for (auto& observer : observers) {
        when(observer) << [] (acquired_cown<unique_ptr<Observer>> observer) {
          (*observer)->notify();
        };
      }
    }

    void write(unique_ptr<T> value) {
      data.push(move(value));
      notify_all();
    }

    unique_ptr<T> read() {
      if (data.size() > 0) {
        unique_ptr<T> front = move(data.front());
        data.pop();
        if (data.size() > 0)
        notify_all();
        return front;
      }
      return nullptr;
    }

    bool has_data() {
      return data.size() != 0;
    }

    void subscribe(cown_ptr<unique_ptr<Observer>> observer) {
      observers.push_back(observer);
      if (data.size() > 0) {
        when(observer) << [] (acquired_cown<unique_ptr<Observer>> observer) {
          (*observer)->notify();
        };
      }
    }
  };

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

  template<typename S1, typename R1, typename S2, typename R2>
    struct Pattern2 {
      cown_ptr<Channel<Message<S1, R1>>> channel1;
      cown_ptr<Channel<Message<S2, R2>>> channel2;

      Pattern2(cown_ptr<Channel<Message<S1, R1>>> channel1,
               cown_ptr<Channel<Message<S2, R2>>> channel2): 
               channel1(channel1), channel2(channel2) {}

      using F = function<void(unique_ptr<Message<S1, R1>>, unique_ptr<Message<S2, R2>>)>;

      struct Pattern : public Observer {
        cown_ptr<Channel<Message<S1, R1>>> channel1;
        cown_ptr<Channel<Message<S2, R2>>> channel2;
        F f;

        Pattern(cown_ptr<Channel<Message<S1, R1>>> channel1,
                cown_ptr<Channel<Message<S2, R2>>> channel2,
                F f): channel1(channel1), channel2(channel2), f(forward<F>(f)) {}
      
        void notify() {
          when(channel1, channel2) << [f=f](acquired_cown<Channel<Message<S1, R1>>> channel1, acquired_cown<Channel<Message<S2, R2>>> channel2) {
          if (!channel1->has_data() || !channel2->has_data())
            return;

          unique_ptr<Message<S1, R1>> msg1 = channel1->read();
          unique_ptr<Message<S2, R2>> msg2 = channel2->read();

          assert(msg1 && msg2);

          f(move(msg1), move(msg2));
          };
        }
      };

      void Do(F run) {
        auto pattern = make_cown<unique_ptr<Observer>>(make_unique<Pattern>(channel1, channel2, forward<F>(run)));
        when(channel1, channel2) << [pattern=pattern](acquired_cown<Channel<Message<S1, R1>>> channel1, acquired_cown<Channel<Message<S2, R2>>> channel2){
          channel1->subscribe(pattern);
          channel2->subscribe(pattern);
        };
      }
    };

  template<typename S, typename R>
  struct Pattern1 {
    cown_ptr<Channel<Message<S, R>>> channel;

    Pattern1(cown_ptr<Channel<Message<S, R>>> channel): channel(channel) {}

    template<typename S2, typename R2>
    Pattern2<S, R, S2, R2> And(cown_ptr<Channel<Message<S2, R2>>> channel2) {
      return Pattern2(channel, channel2);
    }

    using F = function<void(unique_ptr<Message<S, R>>)>;

    struct Pattern : public Observer {
      cown_ptr<Channel<Message<S, R>>> channel;
      F f;

      Pattern(cown_ptr<Channel<Message<S, R>>> channel, F f): channel(channel), f(forward<F>(f)) {}
    
      void notify() {
        when(channel) << [f=f](acquired_cown<Channel<Message<S, R>>> channel) {
          unique_ptr<Message<S, R>> msg = channel->read();
          // if there was a value, call the callback
          if (msg) f(move(msg));
          // otherwise something must have taken it
        };
      }
    };

    void Do(F run) {
      auto pattern = make_cown<unique_ptr<Observer>>(make_unique<Pattern>(channel, forward<F>(run)));
      when(channel) << [pattern=pattern](acquired_cown<Channel<Message<S, R>>> channel){
        channel->subscribe(pattern);
      };
    }
  };

  /*
    Join starts of the construction of a pattern
  */

  namespace Join {
    template<typename S, typename R>
    static Pattern1<S, R> When(cown_ptr<Channel<Message<S, R>>> channel) {
      return Pattern1<S, R>(channel);
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

    when(put) << [](acquired_cown<Channel<DataMessage<int>>> put){
      put->write(make_unique<DataMessage<int>>(make_unique<int>(20)));
    };
    
    when(put) << [](acquired_cown<Channel<DataMessage<int>>> put){
      put->write(make_unique<DataMessage<int>>(make_unique<int>(51)));
    };

    /* send a repliable message on get */
    when(get) << [](acquired_cown<Channel<ReplyMessage<int>>> get){
      get->write(make_unique<ReplyMessage<int>>([](unique_ptr<int> msg) mutable {
        cout << *msg << " -- 1" << endl;
      }));
    };

    /* create a pattern so if there is a message on put then print it */
    Join::When(put).Do([](unique_ptr<DataMessage<int>> msg) {
      cout << **(msg->data) << " -- 0" << endl;
    });

    /* create a pattern so that if there is a message on put and get then reply to get */
    Join::When(put).And(get).Do([](unique_ptr<DataMessage<int>> put, unique_ptr<ReplyMessage<int>> get) {
      (*(get->reply))(move(*(put->data)));
    });

    when(get) << [](acquired_cown<Channel<ReplyMessage<int>>> get){
      get->write(make_unique<ReplyMessage<int>>([](unique_ptr<int> msg) mutable {
        cout << *msg << " -- 2" << endl;
      }));
    };

    when(put) << [](acquired_cown<Channel<DataMessage<int>>> put){
      put->write(make_unique<DataMessage<int>>(make_unique<int>(409)));
    };
  }
}

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(Joins::run);
}