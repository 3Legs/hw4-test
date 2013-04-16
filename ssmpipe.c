#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

/* define macros */
#define __NR_ssmem_attach 	333
#define __NR_ssmem_detach 	334
#define MAX_SSMEM 					1024
#define WRITER 							0
#define READER 							1
#define SSMEM_CREATE  			0x1
#define SSMEM_WRITE   			0x2
#define SSMEM_EXEC    	 		0x4
#define KB									1024

/* define variables */

/* define functions */
int get_num(int i);
int parse_int(char *name);

int main(int argc, char **argv)
{
	char *name = NULL;
	int ssmem_id = -1;
	int type = -1;
	int result = -1;
	int flag = 0;

	if (argc != 4) {
		fprintf(stderr, "ssmpipe must take 4 parameters!\n");
		return -1;
	}

	name = malloc(sizeof(char) * (strlen(argv[1])+1));
	strcpy(name, argv[1]);
	ssmem_id = parse_int(argv[2]);

	if (ssmem_id == -1) {
		fprintf(stderr, "Invalid character in ssmem_id!\n");
		return -1;
	}

	if (ssmem_id > MAX_SSMEM-1 || ssmem_id < 0) {
		fprintf(stderr, "ssmem_id must be between 0 and 1023!\n");
		return -1;
	}

	if (strcmp(argv[3], "writer") == 0) {
		type = WRITER;
	} else if (strcmp(argv[3], "reader") == 0) {
		type = READER;
	} else {
		fprintf(stderr, "Invalid type!\n");
		return -1;
	}

	flag |= (SSMEM_CREATE | SSMEM_WRITE);

	result = syscall(__NR_ssmem_attach, ssmem_id, flag, 8*KB);
	fprintf(stdout, "ADDR: %x\n", result);

	return 0;
}

int get_num(int i)
{
	int ret = 1;
	int p;
	for (p=0; p<i; ++p) {
		ret *= 10;
	}
	return ret;
}

int parse_int(char *name)
{
	int len = strlen(name);
	int ret = 0;
	int i;
	for (i=0; i<len; ++i) {
		if (name[i]<48 || name[i]>57) {
			return -1;
		}
	}
	for (i=0; i<len; ++i) {
		ret += (int)(name[i]-48) * get_num(len-i-1);
	}
	return ret;
}