# Building
The examples and benchmark are built as follows:

Requires cmake-3.18 or higher.

```
> mkdir build
> cd build
> cmake -G Ninja ../
> ninja
```

# Behaviour-Oriented Concurrency Examples
This repository contains examples for the following problems solved using behaviour-oriented concurrency:
* Bank Accounts - illustrative examples for using cowns and whens, in the context of bank account operations
* Dining Philosophers - solution to the dining philosophers problem
* Barriers - coordinating the start of behaviours
* Fibonacci - Divide and conquer style programming
* Channels - a design for channels and inter-behaviour communication
* Santa - solution the santa problem

Each BoC example can be found in `examples/<example>/<example>.cc` and are used to build the `<example>` executable target.

For example:
```
> cat examples/bank/bank.cc
> ./build/bank
```