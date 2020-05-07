#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

//#define DONT_SAY
#include "WebSocket_Server_Basic_for_Linux.h"

void error_handling(const char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

WebSocket_Buffer WebSocket_buf; // Shared memory was used instead because the capacity would be "1MB X client count" when creating a buffer per client

void* handle_clnt(void* arg)
{
	int clnt_sock = *((int*)arg);
	int recvLen = 0, i;

	while ((recvLen = read_for_websocket(clnt_sock, &WebSocket_buf, WebSocket_buf.buf_len)) > 1)
	{
		pthread_mutex_lock(&mutx);

		// edit transaction from here.

        // WebSocket_buf.WebSocket_opcode = WebSocket_OPCODE_TEXT;
        // WebSocket_buf.WebSocket_opcode = WebSocket_OPCODE_BINARY;
		write_for_websocket(clnt_sock, &WebSocket_buf, recvLen);// echo to client

		// *********************************************************************************************************************
		pthread_mutex_unlock(&mutx);
	}

	pthread_mutex_lock(&mutx);
	for (i = 0; i < clnt_cnt; i++)   // remove disconnected client
	{
		if (clnt_sock == clnt_socks[i])
		{
			while (i++ < clnt_cnt - 1)
				clnt_socks[i] = clnt_socks[i + 1];
			break;
		}
	}
	clnt_cnt--;
	pthread_mutex_unlock(&mutx);
	close(clnt_sock);
	return NULL;
}


int main()
{
	int serv_sock, clnt_sock;
	char message[BUF_SIZE];
	int str_len, i;

	struct sockaddr_in serv_adr;
	struct sockaddr_in clnt_adr;
	socklen_t clnt_adr_sz;

	pthread_t t_id;


#ifndef __cplusplus 
	WebSocket_buf.buf = (char*)malloc(OUTPUT_BUFFER_SIZE);
	WebSocket_buf.buf_len = OUTPUT_BUFFER_SIZE;
	WebSocket_buf.WebSocket_opcode = NULL
#endif // !__cplusplus


	pthread_mutex_init(&mutx, NULL);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		error_handling("socket() error");

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(8080);

	// prevent bind error
	{
		int nSockOpt = 1;
		setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &nSockOpt, sizeof(nSockOpt));
	}

	if (bind(serv_sock, (struct sockaddr*) & serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");

	if (listen(serv_sock, 5) == -1)
		error_handling("listen() error");

	if (atexit(goodbye) != 0)
		error_handling("Error in atexit()");

	clnt_adr_sz = sizeof(clnt_adr);

	pthread_t t_id1;
	pthread_create(&t_id1, NULL, input, NULL);
	pthread_detach(t_id1);

	while (1)
	{
		clnt_adr_sz = sizeof(clnt_adr);
		clnt_sock = accept_for_websocket(serv_sock, (struct sockaddr*) & clnt_adr, &clnt_adr_sz);

		pthread_mutex_lock(&mutx);
		clnt_socks[clnt_cnt++] = clnt_sock;

		// ********* edit if you want to do something when the connection is started *******

		// *********************************************************************************


		pthread_mutex_unlock(&mutx);

		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		pthread_detach(t_id);
#ifndef DONT_SAY
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
#endif // !DONT_SAY

	}

	free(WebSocket_buf.buf);

	return 0;
}
