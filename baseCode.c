#define _GNU_SOURCE
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

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

int main (int argc, char **argv) {
	/* Sockets para aguardar conexões, para a conexão de controle e para a conexão de dados */	
	int listen_conn, control_conn, data_conn;
	
	/* Informações sobre o socket (endereço e porta) ficam nesta struct */
	struct sockaddr_in address_info;
	
	/* Armazena tamanho da linha recebida do cliente */
	ssize_t n;
	
	/* Armazena linhas recebidas do cliente */
	char control_line[MAXLINE + 1];
	
	if (argc != 2) {
		printf("Uso: %s PORTA\n", argv[0]);
		printf("Inicia o servidor FTP na porta PORTA, modo TCP\n");
		exit(1);
	}
	
	/* Criação do socket que aguarda conexões */
	if ((listen_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Erro ao inicializar socket!\n");
		exit(2);
	}
	
	/* Inicializando estrutura com informações da conexão */
	bzero(&address_info, sizeof(address_info));
	address_info.sin_family      = AF_INET;
	address_info.sin_addr.s_addr = htonl(INADDR_ANY);
	address_info.sin_port        = htons(atoi(argv[1]));
	
	/* Associando socket ao endereço e porta especificados pela estrutura */
	if (bind(listen_conn, (struct sockaddr *)&address_info, sizeof(address_info)) == -1) {
		perror("Erro ao associar socket!\n");
		exit(3);
	}
	
	/* Coloca o socket para "escutar" */
	if (listen(listen_conn, LISTENQ) == -1) {
		perror("Erro ao iniciar processo de listen!\n");
		exit(4);
	}
	
	printf("[Servidor no ar. Aguardando conexões na porta %s]\n", argv[1]);
	printf("[Para finalizar, pressione CTRL+C ou rode um kill ou killall]\n");

	/* Loop principal */
	for (;;) {
		/* Obtendo próxima conexão */
		if ((control_conn = accept(listen_conn, NULL, NULL)) == -1) {
			perror("Erro ao obter conexão!\n");
			exit(5);
		}

		/* Gerando novo processo para atender paralelamente à conexão obtida */
		if (fork() == 0) {
			printf("[Conexão aberta]\n");
			/* Este processo não ficará ouvindo novas conexões */
			close(listen_conn);

			/* Loop da interação cliente-servidor FTP */
			while ((n = read(control_conn, control_line, MAXLINE)) > 0) {
				printf("Comando de tamanho %d recebido.\n", n);
			}
			
			printf("[Conexão fechada]\n");
			exit(0);
		}
		
		/* O processo pai continua apenas aguardando novas conexões */
		close(control_conn);
	}
	exit(0);
}
