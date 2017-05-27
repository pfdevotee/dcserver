#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "matrix.h"

#define BACKLOG 11
#define BUFSIZE 1000

int ConnectionIsDown;

//Функция ждет широковещательного пакета на порт MATRIXPORT, посылает по адресу отправителя
//этого пакета число ядер на компьютере и возвращает сокет, законнектившийся с отправителем.
int req_proc(int n)
{
	ConnectionIsDown = 0;
	register int s, tcp_s;
	int b;
	struct sockaddr_in sa, tcp_sa;
	unsigned char* buf;
	buf = (unsigned char*)malloc(BUFSIZE);
	bzero(buf, BUFSIZE);
	socklen_t fromlen;
	ssize_t recsize;

//	printf("line 28\n");
	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("req_proc:udp socket");
		exit(EXIT_FAILURE);
	}

	bzero(&sa, sizeof sa);

	sa.sin_family = AF_INET;
	sa.sin_port   = htons(MATRIXPORT);
	if (INADDR_ANY)
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
	fromlen = sizeof(sa);

//	printf("line 42\n");
	if (bind(s, (struct sockaddr *)&sa, sizeof sa) < 0) {
		perror("req_proc:bind");
		exit(EXIT_FAILURE);
	}


	unsigned char hello[20] = "";

	for(;;){
//		printf("line 52\n");
		recsize = recvfrom(s, (void *)buf, BUFSIZE/2, 0, (struct sockaddr *)&sa, &fromlen);
//		printf("line 54\n");
		if (recsize < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
		    exit(EXIT_FAILURE);
		}
		strncpy(hello, buf, 9);
		if(!strcmp(hello, KEYSTRING)){
			close(s);
			break;
		}
	}
	in_port_t port = buf[9] | (buf[10]<<8);
	printf("remote port:%d\n", port);

//	printf("line 67\n");
	if ((tcp_s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("req_proc:tcp socket");
	    exit(EXIT_FAILURE);
	}

	bzero(&tcp_sa, sizeof tcp_sa);

//	printf("line 75\n");
	tcp_sa.sin_family	   = AF_INET;
	tcp_sa.sin_port		   = port;
	tcp_sa.sin_addr.s_addr = sa.sin_addr.s_addr;
	if (connect(tcp_s, (struct sockaddr *)&tcp_sa, sizeof tcp_sa) < 0) {
		perror("req_proc:connect");
		close(tcp_s);
		return -1;
	//	exit(EXIT_FAILURE);
	}

	char msg[128] = "";
	int bytes = 0;

//	printf("line 89\n");
	bytes = write(tcp_s, KEYSTRING, 9);
	bytes = write(tcp_s, &n, sizeof(n));

	free(buf);
	return tcp_s;
}

void get_colnum(int s, int* col1, int* col2)
{
	FILE* f;
	int bytes;

	bytes = read(s, col1, sizeof(int));
	bytes = read(s, col2, sizeof(int));
	printf("col1=%d col2=%d\n", *col1, *col2);
}

MATRIX_T* get_matrix(int s)
{
	int n;			//порядок матрицы
	MATRIX_T* mtrx;
	int err	   = 0;
	int curpos = 0;	//текушая позиция в mtrx->val в байтах
	int i, bytes;
	int nn;			//кол-во чисел в матрице

	if( read(s, &n, sizeof(n)) < 0){
		perror("get_matrix:read n");
		close(s);
		exit(EXIT_FAILURE);
	}
	nn = n*n;

	mtrx = m_new(n, n, &err);
	if(err){
		perror("get_matrix:m_new");
		close(s);
		exit(EXIT_FAILURE);
	}

	while(curpos < nn){
		if( (bytes = read(s, mtrx->val+curpos, sizeof(double)*(nn-curpos)))<0 ){
			perror("get_matrix:read val");
			fprintf(stderr, "problem at curpos=%d\n", curpos);
			close(s);
			m_free(mtrx);
			exit(EXIT_FAILURE);
		}
		if( (bytes == 0) || (bytes % sizeof(double)) ){
			close(s);
			printf("Соединение с клиентом потеряно\n");
			return NULL;
		}
		curpos += bytes/sizeof(double);
	}
	return mtrx;
}

//Функция получает от клиента матрицу и отправлят ему знакочередующуюся сумму миноров с номерами,
//которые укажет клиент.
void m_net_minors(int s, int nth)
{
	MATRIX_T* mtrx;
	int col1, col2;
	long double minorsum;	//знакочередующаяся сумма миноров № col1,..,col2
	int bytes;

	get_colnum(s, &col1, &col2);
	mtrx = get_matrix(s);
	if(mtrx == NULL)
		return;

	minorsum = m_mt_minorsSum(s, mtrx, nth, col1, col2);
	if(ConnectionIsDown){
		ConnectionIsDown = 0;	
		return;
	}
	printf("%.10Le\n", minorsum);
	bytes = send(s, &minorsum, sizeof(minorsum), 0);
	if(close(s) < 0){
		perror("m_net_minors:close()");
		exit(EXIT_FAILURE);
	}
}