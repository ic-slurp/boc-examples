#include <test/harness.h>
#include <cpp/when.h>

using namespace verona::rt;

struct E {};

void test_body()
{
    cown_ptr<E> a = make_cown<E>({});
    when (a) << [](acquired_cown<E>) {};
}


int main(int argc, char** argv)
{
    SystematicTestHarness harness(argc, argv);

    harness.run(test_body);

    return 0;
}