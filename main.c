/* 
 * File:   main.c
 * Author: dmitry
 *
 * Created on 3 Май 2011 г., 17:47
 */

#include <stdio.h>
#include <stdlib.h>
#include "matrix.h"


int main(int argc, char** argv)
{

	int sock;
	MATRIX_T* mtrx;
	int nth;
	char *endptr;

	sigactset();

	if(argc != 2){
		printf("Требуется аргумент - число потоков.\n");
		return 0;
	}
	nth = strtol(argv[1], &endptr, 10);
	if(errno != 0){
		printf("Неверно указано число потоков\n");
		return 0;
	}
	for(;;){
	//	printf("line 32\n");
		if((sock = req_proc(nth)) < 0)	//устанавливаем соединение с клиентом
			continue;
	//	printf("line 35\n");
		m_net_minors(sock, nth);		//выполняем свою часть работы и отправляем на сервер результат
	}

	return 0;
}

