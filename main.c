#include "main.h"

/* Controle do estado da sessão do usuário */
State state = LISTENING;

/* Controle do tipo de transmissão. Tipo inicial ASCII, que não é suportado */
Type type = OTHERS;

/* Sockets para aguardar conexões, para a conexão de controle e para a conexão de dados */	
int listen_conn, control_conn, data_conn, data_conn_client;

/* Endereço do servidor */
short control_port, data_port;
char* ip;

/* Informações sobre o socket (endereço e porta) ficam nesta struct */
struct sockaddr_in address_info;

int main (int argc, char **argv) {
	/* Armazena tamanho da linha recebida do cliente */
	ssize_t n;
	
	/* Armazena linhas recebidas do cliente */
	char control_line[MAX_COMMAND_LINE + 1];
	
	if (argc != 2) {
		printf("Usage: %s <PORT>\n", argv[0]);
		printf("Starts the FTP server on port <PORT>.\n");
		exit(1);
	}
	
	/* Portas do servidor */
	control_port = atoi(argv[1]);
	
	/* Criação do socket que aguarda conexões */
	if ((listen_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Error on socket (listen connection)!\n");
		exit(2);
	}
	
	/* Inicializando estrutura com informações da conexão */
	bzero(&address_info, sizeof(address_info));
	address_info.sin_family      = AF_INET;
	address_info.sin_addr.s_addr = htonl(INADDR_ANY);
	address_info.sin_port        = htons(control_port);
	
	/* Associando socket ao endereço e porta especificados pela estrutura */
	if (bind(listen_conn, (struct sockaddr *)&address_info, sizeof(address_info)) == -1) {
		perror("Error on bind (listen connection)!\n");
		exit(3);
	}
	
	/* Coloca o socket para "escutar" */
	if (listen(listen_conn, LISTEN_QUEUE_SIZE) == -1) {
		perror("Error on listen (Control Connection)!\n");
		exit(4);
	}

	printf("=================================\n");
	printf("= Ridiculously Basic FTP Server =\n");
	printf("=================================\n\n");
	
	printf("Welcome! Listening for connections on port %d\n", control_port);

	/* Loop principal */
	for (;;) {
		/* Obtendo próxima conexão */
		if ((control_conn = accept(listen_conn, NULL, NULL)) == -1) {
			perror("Error on accept (Control Connection)!\n");
			exit(5);
		}
		
		/* Obtendo IP da conexão e substituindo pontos por virgulas*/
		unsigned int size = sizeof(struct sockaddr_in);
		getsockname(control_conn, (struct sockaddr *)&address_info, &size);
		ip = inet_ntoa(address_info.sin_addr);
		int i;
		for (i = 0; i < strlen(ip); ++i) if (ip[i] == '.') ip[i] = ',';

		/* Gerando novo processo para atender paralelamente à conexão obtida */
		if (fork() == 0) {
			printf("[Connection open]\n");
			state = WAITING_USER;

			/* Este processo não ficará ouvindo novas conexões */
			close(listen_conn);
			
			/* Mensagem de boas-vindas */
			char *msg = "220 Welcome to the ridiculously basic FTP server!\n";
			write(control_conn, msg, strlen(msg));
			
			/* Loop da interação cliente-servidor FTP */
			while ((n = read(control_conn, control_line, MAX_COMMAND_LINE)) > 0) {
				/* Descartando os dois últimos caracteres (\r\n) */
				control_line[n - 2] = 0;
				printf("Command required: %s.\n", control_line);

				/* Primeiro argumento é o comando a ser executado */
				char *cmd = strtok(control_line, " ");
				
				/* Seleciona a função que cuida do comando recebido */
				if      (strcasecmp(cmd, "USER") == 0) command_user();
				else if (strcasecmp(cmd, "PASS") == 0) command_pass();
				else if (strcasecmp(cmd, "TYPE") == 0) command_type();
				else if (strcasecmp(cmd, "PASV") == 0) command_pasv();
				else if (strcasecmp(cmd, "RETR") == 0) command_retr();
				else if (strcasecmp(cmd, "STOR") == 0) command_stor();
				else if (strcasecmp(cmd, "QUIT") == 0) command_quit();
				else                                   command_not_implemented();
			}

			/* Fechar sockets ao recebe EOF */
			command_quit();
		}
		
		/* O processo pai continua apenas aguardando novas conexões */
		close(control_conn);
	}
	
	return 0;
}

void command_user() {
	char *msg = "331 User accepted. Password requerida.\n";
	write(control_conn, msg, strlen(msg));
	state = WAITING_PASSWORD;
}

void command_pass() {
	char *msg = "230 User logged in.\n";
	write(control_conn, msg, strlen(msg));
	state = ACTIVE;
}

void command_type() {
	char *arg = strtok(NULL, " ");
	arg[0] = tolower(arg[0]);

	if (arg[0] == 'i') 
		type = IMAGE;
	else 
		type = OTHERS;
	
	char *msg = "200 OK\n";
	write(control_conn, msg, strlen(msg));
}

void command_pasv() {
	char *msg = malloc(50);

	if (state == PASSIVE) {
		sprintf(msg, "227 Passive mode. %s,%hu,%hu\n", ip, data_port >> 8, data_port & 0xFF);
		write(control_conn, msg, strlen(msg));
		free(msg);
		return;
	}

	if ((data_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Error on socket (data connection)!\n");
		exit(2);
	}
	struct sockaddr_in data_address_info;
	bzero(&data_address_info, sizeof(data_address_info));
	data_address_info.sin_family      = AF_INET;
	data_address_info.sin_addr.s_addr = address_info.sin_addr.s_addr;
	data_address_info.sin_port        = htons(0);

	if (bind(data_conn, (struct sockaddr *)&data_address_info, sizeof(data_address_info)) == -1) {
		perror("Error on bind (data connection)!\n");
		exit(3);
	}
	if (listen(data_conn, LISTEN_QUEUE_SIZE) == -1) {
		perror("Error on listen (data connection)!\n");
		exit(4);
	}
	
	/* Obtendo Porta da conexão de dados*/
	unsigned int size = sizeof(struct sockaddr_in);
	getsockname(data_conn, (struct sockaddr *)&data_address_info, &size);
	data_port = ntohs(data_address_info.sin_port);

	sprintf(msg, "227 Passive mode. %s,%hu,%hu\n", ip, data_port >> 8, data_port & 0xFF);
	write(control_conn, msg, strlen(msg));
	free(msg);
	state = PASSIVE;
}

void command_port() {
	char *msg;
	
	if (state < ACTIVE) msg = "530 User not logged in.\n";
	else {
		msg = "200 OK\n";
		state = ACTIVE;
		if (state == PASSIVE)
			close(data_conn);
	}
	write(control_conn, msg, strlen(msg));
}

void command_retr() {
	char *msg;

	if (type == OTHERS) {
		msg = "451 Types other than Image not supported.\n";
		write(control_conn, msg, strlen(msg));
		return;
	}

	char *file_name = strtok(NULL, " ");
	int i;

	FILE *file = fopen(file_name, "rb");

	if (file == NULL) {
		printf("File not found\n");
		msg = "550 File not Found.\n";
		write(control_conn, msg, strlen(msg));
		return;
	} else {
		printf("File opened\n");
		msg = "150 File okay; opening data connection.\n";
		write(control_conn, msg, strlen(msg));
	}

	if (fork() == 0) {
		if ((data_conn_client = accept(data_conn, NULL, NULL)) == -1) {
			perror("Error on accept (data connection)!\n");
			exit(5);
		}

		fseek(file, 0L, SEEK_END);
		int size = ftell(file);
		fseek(file, 0L, SEEK_SET);

		char byte;
		for (i = 0; i < size; ++i) {
			fread(&byte, 1, 1, file);
			write(data_conn_client, &byte, 1);
		}

		close(data_conn_client);
		fclose(file);

		msg = "226 Transfer completed!.\n";
		write(control_conn, msg, strlen(msg));
		exit(0);
	}
}

void command_stor() {
	char *msg;

	if (type == OTHERS) {
		msg = "451 Types other than Image not supported.\n";
		write(control_conn, msg, strlen(msg));
		return;
	}

	char *file_name = strtok(NULL, " ");
	int n;

	FILE *file = fopen(file_name, "wb");

	if (file == NULL) {
		printf("File failed to be created.\n");
		msg = "451 File failed to be created.\n";
		write(control_conn, msg, strlen(msg));
		return;
	} else {
		printf("File created.\n");
		msg = "150 File okay; opening data connection.\n";
		write(control_conn, msg, strlen(msg));
	}

	if (fork() == 0) {
		if ((data_conn_client = accept(data_conn, NULL, NULL)) == -1) {
			perror("Error on accept (data connection)!\n");
			exit(5);
		}

		char byte;
		while ((n = read(data_conn_client, &byte, 1)) > 0) {
			fwrite(&byte, 1, 1, file);
		}

		close(data_conn_client);
		fclose(file);

		msg = "226 Transfer completed.\n";
		write(control_conn, msg, strlen(msg));
		exit(0);
	}
}

void command_quit() {
	char *msg = "221 See ya!\n";
	write(control_conn, msg, strlen(msg));
	
	close(control_conn);
	if (state == PASSIVE)
		close(data_conn);

	printf("[Connection closed]\n");
	exit(0);
}

void command_not_implemented() {
	char *msg = "502 Command not implemented.\n";
	write(control_conn, msg, strlen(msg));
}
