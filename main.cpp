#include "Guid.h"
#include <string>

#include <iostream>
#include <chrono>
#include <Windows.h>

int main(int arg, char** args)
{
    std::string str = "256a4180-65aa-42ec-a945-5fd21dec0538";

    auto start = std::chrono::steady_clock::now();
    int errors = 0;
    for (int i = 0; i < 10000000; i++)
    {
        Guid g;
        errors += g.Parse(str.c_str());
    }

    auto end = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    auto count = (double)diff.count() / 10000000;
    std::cout << "Errors: " << errors << ". Parsed in " << count << " ms\n";

    LPWSTR guidstr = L"{256a4180-65aa-42ec-a945-5fd21dec0538}";

    start = std::chrono::steady_clock::now();
    errors = 0;
    for (int i = 0; i < 10000000; i++)
    {
        GUID guid;
        errors += CLSIDFromString(guidstr, (LPCLSID)&guid);
    }

    end = std::chrono::steady_clock::now();
    diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    count = (double)diff.count() / 10000000;
    std::cout << "Errors: " << errors << ". Parsed in " << count << " ms\n";

    return 0;
}
