all: my_rtt

my_rtt: my_mpi.h my_mpi.c my_rtt.c
	gcc my_mpi.h my_mpi.c my_rtt.c -o ./my_rtt -O3 -lm -lpthread

clean: 
	rm -f my_rtt