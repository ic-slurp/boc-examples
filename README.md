# Behaviour-Oriented Concurrency Examples
This repository contains examples for the following problems solved using behaviour-oriented concurrency:
* Bank Accounts
* Dining Philosophers
* Barriers
* Channels
* Santa

# Behaviour-Oriented Concurrency Benchmark
The dining philosophers benchmark can be found in `verona/src/rt/test/perf/dining_phil/`:
* `pthread_dining_phil.cc` - implemented using pthreads and synchronisation primitives
* `verona_dining_phil.cc` - implemented using cowns and behaviours

Run the benchmark using combinations of the following options:
```
./benchmark
  --hunger <HUNGER>                       # Number of times for a philosopher to eat
  --num_philosophers <NUM_PHILOSOPHERS>   # Number of philosophers
  --optimal_order                         # Schedule philosophers eating in optimal way
  --ponder_usec <USEC>                    # Length of time a philosopher busy waits
  --pthread                               # Use pthread implementation
```
