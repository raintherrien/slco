#include "slco/co.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

CO_BEGIN(generator, { unsigned char i; })
{
	generator->i = 0;
	while (generator->i != UCHAR_MAX) {
		++ generator->i;
		CO_YIELD;
	}
}
CO_END

int main(int argc, char **argv)
{
	CO_DECL(generator) g;
	CO_INIT(generator, &g);

	while (CO_INVOKE(generator, &g) == CO_YIELDED) {
		printf("Yielded: %u\n", g.i);
	}

	return EXIT_SUCCESS;
}
