#include <debug/harness.h>
#include <cpp/when.h>

using namespace verona::rt;

using namespace verona::cpp;

struct E {};

void test_body()
{
    cown_ptr<E> a = make_cown<E>();
    when (a) << [](acquired_cown<E>) {};
}

void test(const cown_ptr<int> x) {
  when(x) <<[](acquired_cown<int> x) {
    
  };
}

int main(int argc, char** argv)
{
    SystematicTestHarness harness(argc, argv);

    harness.run(test_body);

    return 0;
}