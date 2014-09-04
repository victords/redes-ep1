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
	
	printf("[Servidor no ar. Aguardando conexões na porta %d]\n", control_port);
	printf("[Para finalizar, pressione CTRL+C ou rode um kill ou killall]\n");

	/* Loop principal */
	for (;;) {
		/* Obtendo próxima conexão */
		if ((control_conn = accept(listen_conn, NULL, NULL)) == -1) {
			perror("Erro ao obter conexão!\n");
			exit(5);
		}
		
		/* Obtendo IP da conexão e substituindo pontos por virgulas*/
		unsigned int *size = malloc(sizeof(unsigned int));
		*size = sizeof(struct sockaddr_in);
		getsockname(control_conn, (struct sockaddr *)&address_info, size);
		ip = inet_ntoa(address_info.sin_addr);
		int i;
		for (i = 0; i < strlen(ip); ++i) if (ip[i] == '.') ip[i] = ',';

		/* Gerando novo processo para atender paralelamente à conexão obtida */
		if (fork() == 0) {
			printf("[Conexão aberta]\n");
			state = WAITING_USER;

			/* Este processo não ficará ouvindo novas conexões */
			close(listen_conn);
			
			/* Mensagem de boas-vindas */
			char *msg = "220 Bem-vindo ao servidor FTP Ridiculamente Basico!\n";
			write(control_conn, msg, strlen(msg));
			
			/* Loop da interação cliente-servidor FTP */
			while ((n = read(control_conn, control_line, MAX_COMMAND_LINE)) > 0) {
				/* Descartando os dois últimos caracteres (\r\n) */
				control_line[n - 2] = 0;
				printf("Comando recebido: %s.\n", control_line);

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
				else                               command_not_implemented();
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
	char *msg = "331 Usuario aceito. Senha requerida.\n";
	write(control_conn, msg, strlen(msg));
	state = WAITING_PASSWORD;
}

void command_pass() {
	char *msg = "230 Usuario logado.\n";
	write(control_conn, msg, strlen(msg));
	state = ACTIVE;
}

void command_type() {
	char *arg = strtok(NULL, " ");
	arg[0] = tolower(arg[0]);

	char *msg;
	if (arg[0] == 'i') {
		msg = "200 OK\n";
	} else {
		msg = "504 Command not implemented for that parameter.\n";
	}
	write(control_conn, msg, strlen(msg));
}

void command_pasv() {
	char *msg;
	if (state == PASSIVE) {
		msg = "200 OK\n";
		write(control_conn, msg, strlen(msg));
		return;
	}

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
	msg = malloc(50);
	sprintf(msg, "227 Modo passivo. %s,%hhu,%hhu\n", ip, data_port >> 8, data_port);
	write(control_conn, msg, strlen(msg));
	free(msg);
	state = PASSIVE;
}

void command_port() {
	char *msg;
	
	if (state < ACTIVE) msg = "530 Usuario nao logado.\n";
	else {
		msg = "200 OK\n";
		state = ACTIVE;
	}
	write(control_conn, msg, strlen(msg));
}

void command_retr() {
	char *arg = strtok(NULL, " ");
	char *msg;

	int size;
	char *content = read_file(arg, &size);
	if (content == NULL) {
		printf("Arquivo nao encontrado\n");
		msg = "550 File not Found.\n";
		write(control_conn, msg, strlen(msg));
		return;
	} else {
		printf("Arquivo lido de tamanho %d\n", size);
		msg = "150 File status okay; about to open data connection.\n";
		write(control_conn, msg, strlen(msg));
	}

	if ((data_conn_client = accept(data_conn, NULL, NULL)) == -1) {
		perror("Erro ao obter conexão!\n");
		exit(5);
	}

	write(data_conn_client, content, size);

	close(data_conn_client);

	msg = "226 Acabou.\n";
	write(control_conn, msg, strlen(msg));

	free(content);
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

char *read_file(const char *file_name, int *size) {
	FILE *f = fopen(file_name, "rb");
	if (f == NULL)
	    return NULL;

	fseek(f, 0L, SEEK_END);
	*size = ftell(f);
	fseek(f, 0L, SEEK_SET);
	
	char *string = malloc(*size);
	fread(string, 1, *size, f);
	fclose(f);

	return string;
}
