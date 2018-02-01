#include "UUID.h"
#include <string>
#include <iostream>

int main(int arg, char** args)
{
    meyr::UUID guid;

    if (guid.parse("{bebf06de-da19-4f71-b621-cba8865f5a06}"))
        std::cout << guid.to_string().data() << std::endl;
}
