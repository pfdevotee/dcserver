#include "matrix.h"
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

extern int ConnectionIsDown;

typedef struct
{
	pthread_t* p_thread;	//Идентификаторы потоков
	int socket;				//сокет
	int quant;				//кол-во потоков
} threadsinfo;

void* task_for_thread(void* args);
long double m_minor(MATRIX_T* a, int col, double epsilon);
void* connection_state(void* arg);

void s_handler(int arg)	//Обработчик сигнала SIGUSR1
{
	pthread_exit(PTHREAD_CANCELED);
}

void sigactset()
{
	struct sigaction act, oldact;
	bzero(&act, sizeof(act));
	act.sa_handler = s_handler;
	sigfillset(&act.sa_mask);
	if(sigaction(SIGUSR1, &act, NULL) < 0){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}
//Вычисляет дополнительный минор, получаемый вычеркиванием из матрицы a нулевой строки и столбца № col
long double m_minor(MATRIX_T* a, int col, double epsilon)
{
	MATRIX_T* TmpMtrx;
	int err;
	int i, j;
	long double det;

	TmpMtrx = m_new(a->rows-1, a->cols-1, &err);

	for(i = 1; i < a->rows; i++)
		for(j = 0; j < a->cols; j++)
			if(j < col)
				TmpMtrx->val[mdx(TmpMtrx, i-1, j)] = a->val[mdx(a, i, j)];
			else if(j > col)
				TmpMtrx->val[mdx(TmpMtrx, i-1, j-1)] = a->val[mdx(a, i, j)];
			else
				;
	det = m_det(TmpMtrx, epsilon);
	m_free(TmpMtrx);
	return det;
}

//Многопоточная версия функции m_det
//nth - кол-во потоков
long double m_mt_det(MATRIX_T* a, int nth)
{
	int col, err, i;
	int col1, col2;
	long double det, minor;
	long double* minorptr;
	unsigned int args[MAXTHREADS][3];	//Три аргумента каждому потоку
	pthread_t  p_thread[MAXTHREADS]={0};	//Идентификаторы потоков
	pthread_t  p_t;

	det  = 0.0;

	for(i = 1; i < nth; i++){			//Создаем (nth - 1) потоков
		col1 = a->cols*i/nth;
		if(i == nth - 1)
			col2 = a->cols - 1;
		else
			col2 = a->cols*(i+1)/nth - 1;
			
		args[i][0] = col1;
		args[i][1] = col2;
		args[i][2] = (unsigned int)a;
		pthread_create(&p_thread[i], NULL, task_for_thread, args[i]);
	}
	col1 = 0;
	col2 = a->cols/nth - 1;
	args[0][0] = col1;
	args[0][1] = col2;
	args[0][2] = (unsigned int)a;
	minorptr = (long double*)task_for_thread(args[0]);	//main поток тоже не сидит без дела
	det = *minorptr;
	free(minorptr);

	for(i = 1; i < nth; i++){
		pthread_join(p_thread[i], (void**)&minorptr);
		minor = *minorptr;
		det += minor;
		free(minorptr);
	}

	return det;
}

//Функция, выполняемая каждым потоком
//Вычисляет дополнительные миноры, получаемые вычеркиванием из матрицы a нулевой строки и столбцов №№ col1,..,col2
//Аргументы - ук-ли на col1, col2, на ук-ль на матрицу
void* task_for_thread(void* args)
{
	int col1, col2;
	int col, sign;
	MATRIX_T* mtrx;
	long double sminors = 0.0;	//Знакочередующаяся сумма миноров
	long double det;
	double epsilon = 0.0;	//Не используется

	col1 = *((unsigned int*)args);
	col2 = *((unsigned int*)args + 1);
	mtrx = (MATRIX_T*)( *((unsigned int*)args + 2) );

	if(col1&1)
		sign = -1;
	else
		sign = 1;

	for(col = col1; col <= col2; col++) {
		det		 = m_minor(mtrx, col, epsilon);
		sminors += sign*det*mtrx->val[mdx(mtrx, 0, col)];
		sign	 = -sign;
	}
	long double* detptr;
	detptr = (long double*)malloc(sizeof(long double));
	*detptr = sminors;
	
	return detptr;
}


//Функция считает знакочередующуюся сумму миноров матрицы с номерами col1,..,col2
//nth - кол-во потоков
long double m_mt_minorsSum(int sock, MATRIX_T* a, int nth, int col1, int col2)
{
	int col, err, i;
	int thcol1, thcol2;		//номера столбцов для одного потока
	long double det, minor;
	long double* minorptr;
	unsigned int* args[MAXTHREADS];		//Три аргумента каждому потоку
	pthread_t*  p_thread;					//Идентификаторы потоков
	pthread_t  p_t;
	p_thread = (pthread_t*)malloc(nth*sizeof(p_thread));
	

	det  = 0.0;

	for(i = 0; i < nth; i++){			//Создаем nth потоков
		thcol1 = col1 + (col2-col1+1)*i/nth;
		if(i == nth - 1)
			thcol2 = col2;
		else
			thcol2 = col1 + (col2-col1+1)*(i+1)/nth - 1;

		args[i] = (unsigned int*)malloc(3*(sizeof(unsigned int)));
		args[i][0] = thcol1;
		args[i][1] = thcol2;
		args[i][2] = (unsigned int)a;
		pthread_create(&p_thread[i], NULL, task_for_thread, args[i]);
	}

	pthread_t state_pid;
	threadsinfo *th_info = (threadsinfo*)malloc(sizeof(threadsinfo));
	th_info->p_thread	= p_thread;
	th_info->socket		= sock;
	th_info->quant		= nth;
	//Создаем поток, который будет следить за состоянием соединения
	pthread_create(&state_pid, NULL, connection_state, th_info);

	for(i = 0; i < nth; i++){
		pthread_join(p_thread[i], (void**)&minorptr);
	//	free(args[i]);
		minor = *minorptr;
		det += minor;
		free(minorptr);
	}

	free(p_thread);

	pthread_kill(state_pid, SIGUSR1);
/*	if(pthread_kill(state_pid, SIGUSR1)){
		perror("m_mt_minorsSum:kill");
		exit(EXIT_FAILURE);
	}*/
//	free(th_info);
	return det;
}

void* connection_state(void* arg)
{
	threadsinfo* th_info;
	fd_set rfds;
	int retval;

	th_info = (threadsinfo*)arg;
	FD_ZERO(&rfds);
	FD_SET(th_info->socket, &rfds);

	select(th_info->socket + 1, &rfds, NULL, NULL, NULL);
	
	//раз select вернул, значит соединение разорвано

	int i;
	printf("Соединение с клиентом потеряно.\n");
	for(i = 0; i < th_info->quant; i++)
		if(pthread_kill((th_info->p_thread)[i], SIGUSR1)){
			perror("connection_state:kill");
			exit(EXIT_FAILURE);
		}

	ConnectionIsDown = 1;
	close(th_info->socket);
	return NULL;
}