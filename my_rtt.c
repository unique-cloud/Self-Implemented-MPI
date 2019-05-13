/******************************
 * Author : Xiaorui Tang
 * Unity Id : xtang9
 ******************************/
#include "my_mpi.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

#define MIN_SIZE 32
#define MAX_SIZE 2097152
#define ITER_TIMES 11
#define NUM_PAIRS 4
#define ROOT (2*NUM_PAIRS)

#define MSG_TAG 0
#define AVG_TAG 1
#define STDDEV_TAG 2

/**
 * Setup environment, like creating output data file
 */
void setup() {
	printf("Setup the output data file...\n");

	FILE *temp = fopen("data.dat", "w");
	fprintf(temp, "size ");
	for (int i = 0; i < NUM_PAIRS; ++i) {
		fprintf(temp, "pair%d stddev ", i + 1);
	}
	fprintf(temp, "\n");
	fclose(temp);
	return;
}

/**
 * Collect all rtt value together and write into data file
 * @param data_size - the massage data size
 */
void master(int data_size) {
	// Collect rtt result from all processors
	printf("Collecting rtt of data size %d...\n", data_size);

	// Arrays to save the received rtt
	double avg[NUM_PAIRS] = { 0 };
	double stddev[NUM_PAIRS] = { 0 };
	
	for (int i = 0; i < NUM_PAIRS * 2; ++i) {
		double rtt = 0;
		MPI_Status status;
		MPI_Recv(&rtt, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if (status.MPI_TAG == AVG_TAG) {
			// use rank/2 to represent which pair it is
			avg[status.MPI_SOURCE / 2] = rtt;
			printf("Received avg_rtt from rank %d.\n", status.MPI_SOURCE);
		}
		else if (status.MPI_TAG == STDDEV_TAG) {
			stddev[status.MPI_SOURCE / 2] = rtt;
			printf("Received stddev_rtt from rank %d.\n", status.MPI_SOURCE);
		}
		else {}
	}
	
	// Save rtt to data file
	printf("Saving rtt of data size %d...\n", data_size);

	FILE *temp = fopen("data.dat", "a");
	fprintf(temp, "%d ", data_size);
	for (int i = 0; i < NUM_PAIRS; ++i) {
		fprintf(temp, "%le %le ", avg[i], stddev[i]);
	}
	fprintf(temp, "\n");
	fclose(temp);
	return;
}

/**
 * Function to evaluate rtt on each processor.
 * @param data_size	- the massage data size
 * @param rank - rank of current processor
 */
void evaluate_rtt(int data_size, int rank) {
	printf("Start evaluating rtt of data size %d...\n", data_size);
	// Construct msg according to size
	int n = data_size / sizeof(int);
	int msg[n];

	// Initilize rtt result container
	double rtt[ITER_TIMES] = { 0 };
	for (int i = 0; i < ITER_TIMES; ++i) {
		// Use even rank processors as senders, odd rank processors as receivers
		if (rank % 2 == 0) {
			//Only evaluate rtt on sender side
			struct timeval ts;
			struct timeval te;

			gettimeofday(&ts, NULL);
			MPI_Send(msg, n, MPI_INT, rank + 1, MSG_TAG, MPI_COMM_WORLD);
			MPI_Recv(msg, n, MPI_INT, rank + 1, MSG_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			gettimeofday(&te, NULL);

		 	rtt[i] = (te.tv_sec - ts.tv_sec) * 1000000 + te.tv_usec - ts.tv_usec;
		}
		else {
			MPI_Recv(msg, n, MPI_INT, rank - 1, MSG_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Send(msg, n, MPI_INT, rank - 1, MSG_TAG, MPI_COMM_WORLD);
		}
			
	}

	if (rank % 2 == 0) {
		// Caculate avg rtt
		double avg_rtt = 0;
		for (int i = 1; i < ITER_TIMES; ++i) {
			avg_rtt += rtt[i];
		}
		// Skip the first iteration to make result more precise
		avg_rtt /= (ITER_TIMES - 1);

		// Caculate stddev rtt
		double stddev_rtt = 0;
		for (int i = 1; i < ITER_TIMES; ++i) {
			stddev_rtt += (rtt[i] - avg_rtt) * (rtt[i] - avg_rtt);
		}
		stddev_rtt = sqrt(stddev_rtt/(ITER_TIMES - 1));

		// Send result to the ROOT processor
		printf("Send avg rtt from rank %d.\n", rank);
		MPI_Send(&avg_rtt, 1, MPI_DOUBLE, ROOT, AVG_TAG, MPI_COMM_WORLD);
		printf("Send stddev rtt from rank %d.\n", rank);
		MPI_Send(&stddev_rtt, 1, MPI_DOUBLE, ROOT, STDDEV_TAG, MPI_COMM_WORLD);
	}
	return;
}

/**
 * Draw diagram according to the data file
 */
void draw_diagram() {
	printf("Initilize gnuplot...\n");
	// Opens an interface that one can use to send commands as if they were typing into it.
	FILE * gnuplotPipe = popen("gnuplot", "w");
	if (!gnuplotPipe) {
		printf("Gnuplot not found!\n");
		return;
	}

	// Construct gnuplot commands
	char* com_set[] = { "set style data histogram", 
			    "set xlabel 'Message size (bytes)'", 
			    "set ylabel 'Round trip time (microseconds)'",
			    "set style histogram errorbars gap 1",
			    "set xtics rotate by -45", 
			    "set term png",
			    "set output 'p1.png'"
			    };
	
	char com_plot[NUM_PAIRS * 80] = "";
	if(NUM_PAIRS)
		strcat(com_plot, "plot ");

	for (int i = 0; i < NUM_PAIRS; ++i) {
		char tmp[80];
		sprintf(tmp, "'data.dat' using %d:%d:xtic(1) title columnheader(%d), ", 2*i+2, 2*i+3, 2*i+2);
		strcat(com_plot, tmp);
	}
	
	// Check commands
	printf("The commands pending to exec are:\n");
	for (int i = 0; i < sizeof(com_set) / sizeof(com_set[0]); ++i)
		printf("%s\n", com_set[i]);

	printf("%s\n", com_plot);

	// Exec commands to generate the png
	for (int i = 0; i < sizeof(com_set) / sizeof(com_set[0]); ++i)
		// Send commands to gnuplot one by one.
		fprintf(gnuplotPipe, "%s \n", com_set[i]); 

	fprintf(gnuplotPipe, "%s \n", com_plot); 
	fprintf(gnuplotPipe, "exit\n");
	fflush(gnuplotPipe);

	pclose(gnuplotPipe);
	return;
}

int main(int argc, char* argv[]) {
	// Config Check
	if(!NUM_PAIRS) {
		printf("Number of pairs is 0. Program exits.\n");
		return 0;
	}
    
	// Initilize MPI
	MPI_Init(&argc, &argv);
	int size;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	printf("size is %d, rank is %d\n", size, rank);

	// Check if the num of processors matchs the config
	if (size != NUM_PAIRS * 2 + 1) {
		printf("Wrong number of processors!\n");
		return -1;
	}

	// Setup on ROOT processor
	if (rank == ROOT)
		setup();

	for (int data_size = MIN_SIZE; data_size <= MAX_SIZE; data_size *= 2) {
		if (rank == ROOT)
			master(data_size);
		else
			evaluate_rtt(data_size, rank);

		// Use this to make sure the message transformation correct
		MPI_Barrier(MPI_COMM_WORLD);
	}
	// Draw png on ROOT processor
	if (rank == ROOT)
		draw_diagram();

	MPI_Finalize();
	return 0;
}