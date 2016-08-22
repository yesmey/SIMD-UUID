#include "Guid.h"
#include <string>

#include <iostream>
#include <chrono>

int main(int arg, char** args)
{
    std::string str = "256a4180-65aa-42ec-a945-5fd21dec0538";

    auto start = std::chrono::high_resolution_clock::now();
    int len = 0;
    for (int i = 0; i < 10000000; i++)
    {
        Guid g;
        len += g.Parse(str.c_str());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto count = (double)diff.count() / 1000;
    std::cout << "Parsed " << len << " in " << count << " ms\n";

    return 0;
}
