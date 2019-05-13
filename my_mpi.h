#ifndef MY_MPI_H
#define MY_MPI_H

#define MPI_SUCCESS 0
#define MPI_ERROR -1
#define MPI_ANY_SOURCE -1
#define MPI_ANY_TAG -1
#define MPI_STATUS_IGNORE NULL

typedef enum { 
    MPI_INT = 0,
    MPI_DOUBLE = 1,
    MPI_DEFAULT = -1
} MPI_Datatype;

typedef enum { 
    MPI_COMM_WORLD  = 0
} MPI_Comm;

typedef struct {
    int count;
    // int cancelled;
    int MPI_SOURCE;
    int MPI_TAG;
    // int MPI_ERROR;
} MPI_Status;

int MPI_Init(int* argc, char*** argv);

int MPI_Comm_size(MPI_Comm communicator, int* size);
			
int MPI_Comm_rank(MPI_Comm communicator, int* rank);

int MPI_Send(
    void* data,
    int count,
    MPI_Datatype datatype,
    int destination,
    int tag,
    MPI_Comm communicator
    );

int MPI_Recv(
    void* data,
    int count,
    MPI_Datatype datatype,
    int source,
    int tag,
    MPI_Comm communicator,
    MPI_Status* status
    );

int MPI_Barrier();

int MPI_Finalize();

#endif