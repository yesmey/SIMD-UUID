#include "UUID.h"
#include <string>

#include <iostream>
#include <chrono>

#ifdef _MSC_VER

#pragma optimize("", off)
template <class T>
void doNotOptimizeAway(T&& datum) {
    datum = datum;
}
#pragma optimize("", on)

#else
template <class T>
void doNotOptimizeAway(T&& datum) {
    asm volatile("" : "+r" (datum));
}
#endif

int main(int arg, char** args)
{
    std::string str = "53647693-49d7-4da3-aa2e-ad2a8b61a579";

    UUID g;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000000; i++)
    {
        doNotOptimizeAway(std::move(str));
        g.Parse<UUID::BRACKET_NONE, UUID::HEX_CASE_LOWER>(str.c_str());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto count = (double)diff.count() / 1000;
    std::cout << "Errors: " << g.Data()[0] << ". Parsed in " << count << " ms\n";
    return 0;
}
