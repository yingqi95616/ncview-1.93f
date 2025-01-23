#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

main()
{
	printf( "%d", getuid() );
}
