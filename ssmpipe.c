#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


/* define macros */
#define __NR_ssmem_attach 	333
#define __NR_ssmem_detach 	334
#define MAX_SSMEM 			1024
#define WRITER 				0
#define READER 				1
#define SSMEM_CREATE  		0x1
#define SSMEM_WRITE   		0x2
#define SSMEM_EXEC     		0x4
#define KB					1024

/* define variables */
int type = -1;
char *orig_name = NULL;

/* define functions */
int get_num(int i);
int parse_int(char *name);
void tmnt_handler(int sig);

/* define structs */
struct slot_sync {
	/* w_flag is a 1-7 int, denoting the current slot */
	sig_atomic_t w_flag;
	/* w_flag is a 0/1 int, denoting locked/unlocked by the writer */
	sig_atomic_t writing;

	/* r_flag is a int array, denoting whether all readers has finished reading a slot
	 * Each element is a 0-8 int since there are up to 8 readers
	 */
	sig_atomic_t r_flag[8];
	int garbage[246];
};

struct slot_t {
	char line[1024];
};


int main(int argc, char **argv)
{
	char *name = NULL;
	int ssmem_id = -1;
	//int type = -1;
	void *result = NULL;
	int flag = 0;
	sig_atomic_t cur_slot = 0;
	int name_len = 0;

	/* install termination (Ctrl-C and kill) handler */
	signal (SIGINT, tmnt_handler);
	signal (SIGTERM, tmnt_handler);
	//if (signal (SIGINT, tmnt_handler) == SIG_IGN)
    //	signal (SIGINT, SIG_IGN);
    //if (signal (SIGTERM, tmnt_handler) == SIG_IGN)
    //	signal (SIGTERM, SIG_IGN);

	if (argc != 4) {
		fprintf(stderr, "ssmpipe must take 4 parameters!\n");
		return -1;
	}

	name = malloc(sizeof(char) * (strlen(argv[1])+1));
	strcpy(name, argv[1]);
	orig_name = malloc(sizeof(char) * (strlen(argv[1])+1));
	strcpy(orig_name, argv[1]);
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
		flag |= (SSMEM_CREATE | SSMEM_WRITE);
	} else if (strcmp(argv[3], "reader") == 0) {
		type = READER;
		//flag |= (SSMEM_EXEC | SSMEM_WRITE);
		flag |= (SSMEM_CREATE | SSMEM_WRITE);
	} else {
		fprintf(stderr, "Invalid type!\n");
		return -1;
	}

	result = (void *) syscall(__NR_ssmem_attach, ssmem_id, flag, 8*KB);
	//fprintf(stdout, "ADDR: %x\n", result);
	if ((int) result < 0 && (int) result > -138) {
		printf("Error occured calling ssmem_attach!\n");
		return -1;
	}
	

	//result = malloc(8*KB);
	//result = memset(result, 0, 8*KB);

	struct slot_sync *slot_0 = (struct slot_sync *) result;
	struct slot_t *slot_1 = (struct slot_t *) (slot_0 + 1);
	struct slot_t *slot = slot_1;

/*
	printf("sizeof slot_sync: %d\n", sizeof(struct slot_sync));
	printf("sizeof slot_t: %d\n", sizeof(struct slot_t));
	printf("slot_0: %p\n", slot_0);
	printf("slot_1: %p\n", slot_1);
*/

	if (type == READER) {
		while (1) {
			if (cur_slot == 0) {
					cur_slot = slot_0->w_flag;
					slot = slot_1 + cur_slot - 1;
					//printf("begin sleep to wait, and the new slot is %d\n", cur_slot);
					sleep(3);
					continue;
				}
			if (cur_slot != slot_0->w_flag || slot_0->writing == 1) {	
				/* Writer is writing on another slot or is not writing */
				/* Lock the slot: only readable */
				++ slot_0->r_flag[cur_slot];
				
				/* Start reading */
				printf("%s: %s\n", name, slot->line);
				//sleep(20);

				/* Finished reading */
				-- slot_0->r_flag[cur_slot];
				cur_slot = 0;
			}
		}		
	}

	if (type == WRITER) {	
		/* Assume name is shorter than 1024-6 */
		name = strcat(name, " says ");
		name_len = strlen(name);
		cur_slot = slot_0->w_flag;
		while(1) {
			if (cur_slot < 0 || cur_slot > 7) {
				fprintf(stderr, "Slot out of bound!\n");
				return -1;
			}
			if (cur_slot > 6) {
				cur_slot = 1;
				//printf("Back to 1: current slot is %d\n", cur_slot);
			}
			else {
				++cur_slot;
				//printf("current slot is %d\n", cur_slot);
			}
			slot = slot_1 + cur_slot - 1;
			slot_0->w_flag = cur_slot;

			/* Finished writing in the previous loop and unlock the previous slot */	
			slot_0->writing = 1;
			//printf("writing is good\n");
			
			/* Wait for all readers finish reading the certain slot*/
			while(1) {
				//printf("looping\n");
				if (slot_0->r_flag[cur_slot] > 0) {
					//printf("don't be here\n");
					printf("%d readers is/are still reading slot %d\n", slot_0->r_flag[cur_slot], cur_slot);
					sleep(1);
				}
				else {
					/* Lock the slot and set the w-flag */
					slot_0->writing = 0;
					break;
				}
			}

			/* Start writing */
			printf("start writing\n");
			//char *test = "write sth";
			//*(slot->line) = 'a'; 
			memset(slot->line,0,KB);
			memcpy(slot->line, name, name_len);
			slot->line;
			fgets((slot->line) + name_len, KB - name_len - 1, stdin);			
			//printf("In slot[%d] We wrote: %s\n", cur_slot, slot->line);
		}
	}
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

void  tmnt_handler(int sig)
{
	char  c;

	signal(sig, SIG_IGN);
	if (type == WRITER)
		printf("The writer %s has quited.\n", orig_name);
	else if (type == READER)
		printf("The reader %s has quited.\n", orig_name);
	else
		printf("%s has quited.\n", orig_name);

	_exit(0);
}