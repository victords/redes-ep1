#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#define LISTEN_QUEUE_SIZE 1
#define MAX_COMMAND_LINE 1024

char *read_file(const char *file_name);

void command_user();
void command_pass();
void command_pasv();
void command_retr();
void command_stor();
void command_quit();
void command_not_implemented();

typedef enum {
	LISTENING,
	WAITING_USER,
	WAITING_PASSWORD,
	ACTIVE,
	PASSIVE
} State;

