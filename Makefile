dcserver: *.c
	gcc -g $^ -o $@ -lm -lpthread
