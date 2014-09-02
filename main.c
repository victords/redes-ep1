#include "main.h"

/* Controle do estado da sessão do usuário */
State state = LISTENING;

/* Sockets para aguardar conexões, para a conexão de controle e para a conexão de dados */	
int listen_conn, control_conn, data_conn, data_conn_client;

/* Endereço do servidor */
int control_port, data_port;
char* ip;

/* Informações sobre o socket (endereço e porta) ficam nesta struct */
struct sockaddr_in address_info;

int main (int argc, char **argv) {
	/* Armazena tamanho da linha recebida do cliente */
	ssize_t n;
	
	/* Armazena linhas recebidas do cliente */
	char control_line[MAX_COMMAND_LINE + 1];
	
	/********************************
	 * Variáveis de controle do FTP *
	 ********************************/
	char *msg, *cmd;
	
	if (argc != 2) {
		printf("Uso: %s PORTA\n", argv[0]);
		printf("Inicia o servidor FTP na porta PORTA, modo TCP\n");
		exit(1);
	}
	
	/* Portas do servidor */
	control_port = atoi(argv[1]);
	data_port = control_port - 1;
	
	/* Criação do socket que aguarda conexões */
	if ((listen_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Erro ao inicializar socket!\n");
		exit(2);
	}
	
	/* Inicializando estrutura com informações da conexão */
	bzero(&address_info, sizeof(address_info));
	address_info.sin_family      = AF_INET;
	address_info.sin_addr.s_addr = htonl(INADDR_ANY);
	address_info.sin_port        = htons(control_port);
	
	/* Associando socket ao endereço e porta especificados pela estrutura */
	if (bind(listen_conn, (struct sockaddr *)&address_info, sizeof(address_info)) == -1) {
		perror("Erro ao associar socket!\n");
		exit(3);
	}
	
	/* Coloca o socket para "escutar" */
	if (listen(listen_conn, LISTEN_QUEUE_SIZE) == -1) {
		perror("Erro ao iniciar processo de listen!\n");
		exit(4);
	}
	
	printf("[Servidor no ar. Aguardando conexões na porta %s]\n", argv[1]);
	printf("[Para finalizar, pressione CTRL+C ou rode um kill ou killall]\n");
	
	state = LISTENING;
	/* Loop principal */
	for (;;) {
		/* Obtendo próxima conexão */
		if ((control_conn = accept(listen_conn, NULL, NULL)) == -1) {
			perror("Erro ao obter conexão!\n");
			exit(5);
		}
		
		/* Obtendo IP do servidor e substituindo pontos por virgulas*/
		unsigned int *size = malloc(sizeof(unsigned int));
		*size = sizeof(struct sockaddr_in);
		getsockname(control_conn, (struct sockaddr *)&address_info, size);
		ip = inet_ntoa(address_info.sin_addr);
		int i;
		for (i = 0; i < strlen(ip); ++i)
			if (ip[i] == '.') ip[i] = ',';

		/* Gerando novo processo para atender paralelamente à conexão obtida */
		if (fork() == 0) {
			printf("[Conexão aberta]\n");
			/* Este processo não ficará ouvindo novas conexões */
			state = WAITING_USER;
			close(listen_conn);
			
			/* Mensagem de boas-vindas */
			msg = malloc(100);
			sprintf(msg, "220 Bem-vindo ao servidor FTP Ridiculamente Básico!\n");
			write(control_conn, msg, strlen(msg));
			cmd = malloc(5);
			
			/* Loop da interação cliente-servidor FTP */
			while ((n = read(control_conn, control_line, MAX_COMMAND_LINE)) > 0) {
				/* Descartando os dois últimos caracteres (\r\n) */
				control_line[n - 2] = 0;
				cmd = strtok(control_line, " ");
				
				printf("Comando recebido: %s.\n", control_line);
				
				if      (strcmp(cmd, "USER") == 0) command_user();
				else if (strcmp(cmd, "PASS") == 0) command_pass();
				else if (strcmp(cmd, "PASV") == 0) command_pasv();
				else if (strcmp(cmd, "RETR") == 0) command_retr();
				else if (strcmp(cmd, "STOR") == 0) command_stor();
				else if (strcmp(cmd, "QUIT") == 0) command_quit();
				else                               command_not_implemented();
								
/*				if (logged) {*/
/*					if (passive) {*/
/*						if (strcmp(cmd, "RETR") == 0) {*/
/*							char *content = read_file(control_line + 5);*/
/*							printf("Arquivo lido de tamanho %d\n", strlen(content));*/
/*							*/
/*							if ((data_conn_client = accept(data_conn, NULL, NULL)) == -1) {*/
/*								perror("Erro ao obter conexão!\n");*/
/*								exit(5);*/
/*							}*/
/*							*/
/*							write(data_conn_client, content, strlen(content));*/
/*							sprintf(msg, "226 Acabou.\n");*/
/*							write(control_conn, msg, strlen(msg));*/
/*							close(data_conn_client);*/
/*							*/
/*							free(content);*/
/*						} else if (strcmp(cmd, "PASV") == 0) {*/
/*							sprintf(msg, "227\n");*/
/*							write(control_conn, msg, strlen(msg));*/
/*						} else {*/
/*							sprintf(msg, "502 Comando não implementado.\n");*/
/*							write(control_conn, msg, strlen(msg));*/
/*						}*/
/*					} else if (strcmp(cmd, "PASV")) {*/
/*						sprintf(msg, "502 Você deve entrar no modo passivo.\n");*/
/*						write(control_conn, msg, strlen(msg));*/
/*					} else {*/
						
/*					}*/
/*				} else if (strcmp(cmd, "USER")) {*/
/*					sprintf(msg, "530 O primeiro comando deve ser USER.\n");*/
/*					write(control_conn, msg, strlen(msg));*/
/*				} else {*/
/*				}*/
			}
			
			command_quit();
		}
		
		/* O processo pai continua apenas aguardando novas conexões */
		close(control_conn);
	}
	
	return 0;
}

void command_user() {
	char *msg = "331 Usuario aceito. Senha requerida.\n";
	write(control_conn, msg, strlen(msg));
	state = WAITING_PASSWORD;
}

void command_pass() {
	char *msg = "230 Usuario logado.\n";
	write(control_conn, msg, strlen(msg));
	state = ACTIVE;
}

void command_pasv() {
	if ((data_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Erro ao inicializar socket!\n");
		exit(2);
	}
	struct sockaddr_in data_address_info;
	bzero(&data_address_info, sizeof(data_address_info));
	data_address_info.sin_family      = AF_INET;
	data_address_info.sin_addr.s_addr = address_info.sin_addr.s_addr;
	data_address_info.sin_port        = htons(data_port);
	if (bind(data_conn, (struct sockaddr *)&data_address_info, sizeof(data_address_info)) == -1) {
		perror("Erro ao associar socket!\n");
		exit(3);
	}
	if (listen(data_conn, LISTEN_QUEUE_SIZE) == -1) {
		perror("Erro ao iniciar processo de listen!\n");
		exit(4);
	}
	char *msg = malloc(50);
	sprintf(msg, "227 Modo passivo. %s,%hhu,%hhu\n", ip, data_port >> 8, data_port);
	printf("msg: %s", msg);
	write(control_conn, msg, strlen(msg));
	free(msg);
	state = PASSIVE;
}

void command_port() {
	if (state < ACTIVE)
	char *msg = "230 Usuario logado.\n";
	write(control_conn, msg, strlen(msg));
	state = ACTIVE;
}

void command_retr() {
}

void command_stor() {
}

void command_quit() {
	char *msg = "221 Ateh mais!\n";
	write(control_conn, msg, strlen(msg));
	
	close(control_conn);
	printf("[Conexao fechada]\n");
	exit(0);
}

void command_not_implemented() {
	char *msg = "502 Comando nao implementado.\n";
	write(control_conn, msg, strlen(msg));
}

char *read_file(const char *file_name) {
	FILE *f = fopen(file_name, "r");
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *string = malloc(fsize + 2);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = EOF;
	string[fsize + 1] = 0;
	return string;
}

