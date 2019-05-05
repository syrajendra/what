#include <iostream>

int main(int argc, char *argv[])
{
	std::cout << "Hello World" << std::endl;
	char *ptr = NULL;
	/* crash */
	ptr[0] = 'a';
	return 0;
}