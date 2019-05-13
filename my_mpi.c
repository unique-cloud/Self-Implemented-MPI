/******************************
 * Author : Xiaorui Tang
 * Unity Id : xtang9
 ******************************/
#include "my_mpi.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

#define MASTER_RANK 0
#define BARRIER_TAG -2

#define MAX_HOSTNAME_LEN 100
#define MAX_MSG_NUM 100
#define MAX_LINE 1024
#define MAX_IP_LEN 20

#define PORT 18000
#define TIMEOUT 10


/* global variables */
int rank = -1;
int size = 0;
int sockfd = -1;
pthread_t tid;
char** ip_list;

/* Message struct that MPI_Send & MPI_Recv use */
typedef struct {
    size_t msg_size;
    MPI_Datatype datatype;
    int source;
    int tag;
    int count;
    char* data[0];
} MPI_Message;

/* Global buffer to temporarily save received message */
MPI_Message* g_msg_buff[MAX_MSG_NUM];

/* Cleanup func for recv_data thread */
void cleanup_handler(void *arg )
{
   close(sockfd);
   sockfd = -1;
}

/* Thread func that receive message from other nodes via socket */
void recv_data(void *ptr) {
    /* These are for pthread cancel */
    pthread_cleanup_push(cleanup_handler, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    int sockfd, new_fd;
    struct sockaddr_in serv_addr;
    int opt = 1;

    /* Create socket and listen on it */
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{
		printf("socket error!\n");
		exit(-1);
	}
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
    { 
        printf("set socket opt error!\n"); 
        exit(-1); 
    } 

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);
 
	if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0)
	{
		printf("bind error!\n");
        printf("error code is %d\n", errno);
		exit(-1);
	}
	if(listen(sockfd,SOMAXCONN) < 0)
	{
		printf("listen error!\n");
		exit(-1);
	}

    /* Accpet message from other nodes */
    while(1)
	{
		/* accept from specific client */
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_size = sizeof(clnt_addr);
		if((new_fd = accept(sockfd,(struct sockaddr *)&clnt_addr,(socklen_t *)&clnt_addr_size)) < 0)
		{
			printf("accept error!\n");
			exit(-1);
		}

        /* first, get the size of total message */
		size_t msg_size = 0;   //total message length
		if(recv(new_fd, (void*)&msg_size, sizeof(msg_size), 0) < 0)
		{
			printf("recv error!\n");
			exit(-1);
		}

        /* caculate remaining size of message */
		size_t left_size = msg_size - sizeof(msg_size);
		void* data_buf = (void*)malloc(left_size);
		memset(data_buf, 0, left_size);

        /* receive remaining message */
		if(recv(new_fd, data_buf, left_size, 0) < 0)
		{
			printf("recv error!\n");
			exit(-1);
		}
		close(new_fd);

        /* store the message in global buffer */
		MPI_Message* m = (MPI_Message*)malloc(msg_size);
		m->msg_size = msg_size;

		memcpy((void*)(&m->datatype), data_buf, left_size);
        free(data_buf);
        data_buf = NULL;

        for(int i = 0; i < MAX_MSG_NUM; i++) {
            if(g_msg_buff[i] == NULL) {
                g_msg_buff[i] = m;
                break;
            }
            if(i == MAX_MSG_NUM - 1) {
                printf("exceed global message amount limit!\n");
                exit(-1);
            }
        }
	}

    pthread_cleanup_pop(1);
}

/* Init recv_data thread */
int init_socket_thread() {

    int ret = pthread_create(&tid, NULL, (void *)&recv_data, NULL);
    if(ret)
        return -1;
        
    return 0;
}

/* Write hostname to shared file
 * Use line number of hostname as rank
 */
int write_hostname(char* hostname) {

    FILE *fp;
    if((fp = fopen("node_list.txt", "a+")) == NULL)    
        return -1;

    /* add '\n' for each hostname, so that rank is equal to line number */
    fprintf(fp, "%s\n", hostname);
    fclose(fp);

    return 0;
}

/* get total line number of file as world size */
int get_world_size() {

    FILE *fp;
    if((fp = fopen("node_list.txt", "r")) == NULL)    
        return -1;

    /* count the total line number as size*/
    int count = 0;
    char str[MAX_HOSTNAME_LEN] = {0};
    while(fgets(str, MAX_LINE, fp) != NULL)   // set enough length to make sure fgets() read a whole line
            count++;

    fclose(fp);  
    return count;
}

/* get line number of hostname as rank */
int get_node_rank(char* hostname) {

    FILE *fp;
    if((fp = fopen("node_list.txt", "r")) == NULL)    
        return -1;

    /* count target's line number(start from 0) as rank */
    int count = 0;  
    char str[MAX_HOSTNAME_LEN] = {0};
    char target[MAX_HOSTNAME_LEN] = {0};

    strcat(target, hostname);
    strcat(target, "\n");

    while(fgets(str, MAX_LINE, fp) != NULL) {   // set enough length to make sure fgets() read a whole line
        /* return line number when target found */
        if (strcmp(str, target) == 0) {
            fclose(fp);
            return count;
        }
        count++;
    }
    
    return -1;
}

/* use line number to get the hostname */
int get_node_hostname(int rank, char* hostname) {

    FILE *fp;
    if((fp = fopen("node_list.txt", "r")) == NULL)    
        return -1;

    /* count current line number */
    int count = 0;  
    char str[MAX_HOSTNAME_LEN] = {0};

    /* read a whole line every iteration */
    while(fgets(str, MAX_HOSTNAME_LEN + 1, fp) != NULL) {   // set enough length to make sure fgets() read a whole line
        if(rank == count) {
            /* delete the last '\n' character */
            int len = strlen(str);
            str[len -1] = '\0';
            strcpy(hostname, str);

            fclose(fp);
            return 0;
        }

        count++;
    }
    
    return -1;
}

/* reslove ip by rank */
int get_ip(int destination, char* ip) {

    /* get hostname by rank */
    char hostname[MAX_HOSTNAME_LEN] = {0};  
    get_node_hostname(destination, hostname);
 
    /* get ip by hostname */
    struct hostent *hp;
    if ((hp = gethostbyname(hostname)) == NULL)
            return -1;

    strcpy(ip, inet_ntoa(*(struct in_addr*)hp->h_addr_list[0]));

    return 0;
}

int setup_hostlist() {
 
    ip_list = (char**)malloc(sizeof(char*) * size);
    for(int i = 0; i < size; i++) {
        char* ip = (char*)malloc(sizeof(char) * MAX_IP_LEN);
        memset(ip, 0, sizeof(char) * MAX_IP_LEN);

        if(get_ip(i, ip)) {
            printf("resolve ip error\n");
            exit(-1);
        }

        ip_list[i] = ip;
    }
}

int free_hostlist() {
    for(int i = 0; i < size; i++) {
        free(ip_list[i]);
    }

    free(ip_list);
    return 0;
}

int lookup_ip(int rank, char* ip) {
    if(rank >= size)
        return -1;
    
    strcpy(ip, ip[rank]);
    return 0;
}

int MPI_Init(int* argc, char*** argv) {
    if(*argc < 2) {
		printf("Lack parameters! Please specify number of nodes.\n");
	}

    /* get the size from command line */
    for(int i = 0; i < *argc - 1; i++) {
        if(strcmp((*argv)[i], "-n") == 0) {
            size = atoi((*argv)[i+1]);
            break;
        }
    }
    if(size == 0) {
        printf("get size error!\n");
        exit(-1);
    }

    /* start a socket thread to recieve data */
    if(init_socket_thread() == -1) {
        printf("create socket thread error!\n");
        exit(-1);
    }

    /* get & write the hostname to shared file */
    char host[MAX_HOSTNAME_LEN] = {0};
    if(gethostname(host, sizeof(host)) == -1) {
        printf("get host name error!\n");
        exit(-1);
    }
    if(write_hostname(host) == -1) {
        printf("write host name error!\n");
        exit(-1);
    }

    /* match size with input parameter so that we know every node init successfully */
    /* set TIMEOUT for 10s, this is the waiting time for other nodes writing hostname */
    int sec_1 = time((time_t*)NULL);
    int num = 0, sec_2 = 0;
    while((num = get_world_size()) != size) {
        sec_2 = time((time_t*)NULL);
        if((sec_2 - sec_1) == TIMEOUT) {
            printf("MPI init failed!\n");
            exit(-1);
        }
    }
    rank = get_node_rank(host);

    /* setup ip lookup table */
    setup_hostlist();

        /* Remove node list file */
    if(rank == MASTER_RANK)
        remove("node_list.txt");

    return MPI_SUCCESS;
}

int MPI_Comm_size(MPI_Comm communicator, int* my_size) {

    *my_size = size;
    return MPI_SUCCESS;
}
			
int MPI_Comm_rank(MPI_Comm communicator, int* my_rank) {

    *my_rank = rank;
    return MPI_SUCCESS;
}

int MPI_Send(
    void* data,
    int count,
    MPI_Datatype datatype,
    int destination,
    int tag,
    MPI_Comm communicator
    )
{
    /* resolve ip by rank*/
    char ip[20];
    if(lookup_ip(destination, ip) == -1) {
        printf("get ip error!\n");
        exit(-1);
    }

    /* initialize socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)  
    { 
        printf("invalid ip address!\n"); 
        exit(-1);
    } 

    /* connect */
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("connection failed! ip is %s\n", ip); 
        printf("error number is %d\n", errno);
        exit(1);
    } 

    /* pack message */
    size_t data_len = 0;
    switch(datatype) {
        case MPI_INT :
            data_len = sizeof(int) * count;
            break;
        case MPI_DOUBLE :
            data_len = sizeof(double) * count;
            break;
        default :
            break;
    }

    size_t msg_size = sizeof(MPI_Message) + data_len;
    MPI_Message* m = (MPI_Message*)malloc(msg_size);
    m->msg_size = msg_size;
    m->datatype = datatype;
    m->source = rank;
    m->tag = tag;
    m->count = count;

    /* NULL pointer is supported*/
    if(data)
        memcpy((void*)m->data, data, data_len);

    /* send message */
    if(send(sock, (void*)m, msg_size, 0) == -1) {
        printf("send message error!\n");
        exit(-1);
    }

    close(sock);
    free(m);
    return MPI_SUCCESS;
}

/* Retrieve message from global buffer */
int MPI_Recv(
    void* data,
    int count,
    MPI_Datatype datatype,
    int source,
    int tag,
    MPI_Comm communicator,
    MPI_Status* status
    )
{
    MPI_Message* m = NULL;
    /* block until data recieved */
    while(m == NULL) {
        for(int i = 0; i < MAX_MSG_NUM; i++) {
            if(g_msg_buff[i] != NULL) {
                /* msg found when both source and tag matched */
                if((source == MPI_ANY_SOURCE || source == g_msg_buff[i]->source) &&
                    (tag == MPI_ANY_TAG || tag == g_msg_buff[i]->tag))
                {
                    m = g_msg_buff[i];
                    g_msg_buff[i] = NULL;
                    break;
                }
            }
        }
    }

    /* validity check */
    if(m->datatype != datatype || m->count != count) {
        printf("datatype or count mismatch!\n");
        exit(-1);
    }

    /* copy data */
    size_t data_len = 0;     
    switch(datatype) {
        case MPI_INT :
            data_len = sizeof(int) * count;
            break;
        case MPI_DOUBLE :
            data_len = sizeof(double) * count;
            break;
        default :
            data_len = 0;
            break;
    }
    
    if(data)
        memcpy(data, m->data, data_len);

    if(status != MPI_STATUS_IGNORE) {
        status->count = m->count;
        status->MPI_SOURCE = m->source;
        status->MPI_TAG = m->tag;
    }

    free(m);
    return MPI_SUCCESS;
}

/* synchronize nodes */
int MPI_Barrier() {
    /* first, send message to the master rank */
    MPI_Send(NULL, 0, MPI_DEFAULT, MASTER_RANK, BARRIER_TAG, MPI_COMM_WORLD);
    
    if(rank == MASTER_RANK) {
        /* after master rank receives message from all nodes */
        for(int i = 0; i < size; i++) {
            MPI_Recv(NULL, 0, MPI_DEFAULT, i, BARRIER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        /* send message back to notify synchronization success */
        for(int j = 0; j < size; j++) {
            MPI_Send(NULL, 0, MPI_DEFAULT, j, BARRIER_TAG, MPI_COMM_WORLD);
        }
    }

    /* receive success message then go on */
    MPI_Recv(NULL, 0, MPI_DEFAULT, MASTER_RANK, BARRIER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    return MPI_SUCCESS;
}

int MPI_Finalize() {
    
    MPI_Barrier();

    /* Clean up */
    if (pthread_cancel(tid))
    {
        printf("pthread cancel error!\n");
        exit(-1);
    }
    pthread_join(tid, NULL);

    free_hostlist();
    rank = -1;
    size = 0;
    return MPI_SUCCESS;
}