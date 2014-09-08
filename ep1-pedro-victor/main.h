#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#define LISTEN_QUEUE_SIZE 1
#define MAX_COMMAND_LINE 1024

void command_user();
void command_pass();
void command_type();
void command_pasv();
void command_port();
void command_retr();
void command_stor();
void command_quit();
void command_not_implemented();
char not_logged();

typedef enum {
	LISTENING,
	WAITING_USER,
	WAITING_PASSWORD,
	ACTIVE,
	PASSIVE,
	TRANSFERRING
} State;

typedef enum {
	IMAGE,
	OTHERS,
} Type;
