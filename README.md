# Building
The examples and benchmark are built as follows:
```
> mkdir build
> cd build
> cmake -G Ninja ../
> ninja
```

# Behaviour-Oriented Concurrency Examples
This repository contains examples for the following problems solved using behaviour-oriented concurrency:
* Bank Accounts
* Dining Philosophers
* Barriers
* Fibonacci
* Channels
* Santa

Each BoC example can be found in `examples/<example>/<example>.cc` and are used to build the `<example>` executable target.

For example:
```
> cat examples/bank/bank.cc
> ./build/bank
```

# Behaviour-Oriented Concurrency Benchmark
The dining philosophers benchmark can be found in `verona/src/rt/test/perf/dining_phil/`:
* `pthread_dining_phil.cc` - implemented using pthreads and synchronisation primitives
* `verona_dining_phil.cc` - implemented using cowns and behaviours

Run the benchmark using combinations of the following options:
```
> ./build/benchmark
  --hunger <HUNGER>                       # Number of times for a philosopher to eat
  --num_philosophers <NUM_PHILOSOPHERS>   # Number of philosophers
  --optimal_order                         # Schedule philosophers eating in optimal way
  --ponder_usec <USEC>                    # Length of time a philosopher busy waits
  --pthread                               # Use pthread implementation
  --manual_lock_order                     # Use less than lock order acquisition instead of std::lock backoff
```

The interesting configurations can be run, and the results plotted, using the scripts in `scripts/`:
```
> mkdir results
> python3 ./scripts/dining.py --benchmark=./build/benchmark -o results/ # runs the benchmarks, saves timing csv results and graph into results/
```