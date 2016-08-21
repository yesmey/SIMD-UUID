#include "Guid.h"
#include <string>


int main(int arg, char** args)
{
	std::string str = "c56a4180-65aa-42ec-a945-5fd21dec0538";
	Guid g;
	g.Parse(str.c_str());
	return 0;
}
