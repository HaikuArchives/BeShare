// This little utility lets you chop the last <n> bytes off
// of a file easily.  Default is to chop the last 64K off.
// Handy if you've got a big file that BeShare won't resume
// downloading on because the last bit got messed up during
// a disk crash.
//
// Compile thus:  gcc filechopper.cpp
//
// Usage:  filechopper <filename> [numbytestoremove=65536]
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int main(int argc, char ** argv)
{
	const long defaultChop = 65536;
	if ((argc != 3) && (argc != 2)) {
		printf("filechopper removes the last (n) bytes from the file you\n");
		printf("specify, which can be useful if your machine/net_server crashed\n");
		printf("during download and it won't resume now (because it has\n");
		printf("garbage bytes in it from the crash)\n");
		printf("usage:  filechopper <filename> [numbytestoremove=%li]\n", defaultChop);
		exit(0);
	}

	long count = (argc == 3) ? atol(argv[2]) : defaultChop;
	FILE * fp = fopen(argv[1], "r");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long fileLen = ftell(fp);
		fclose(fp);

		int fd = open(argv[1], O_RDWR|O_APPEND); 
		if (fd >= 0) {
			long newSize = fileLen - count;
			if (newSize >= 0) {
				ftruncate(fd, newSize);
				printf("Chopped the last %li bytes off of [%s], new size is [%li]\n", count, argv[1], newSize);
			} else
				printf("File is too short to chop that many bytes off!\n");
		} else
			printf("Error, file [%s] couldn't be opened???\n", argv[1]);
	} else
		printf("Error, file [%s] not found\n", argv[1]);

	return 0;
}
