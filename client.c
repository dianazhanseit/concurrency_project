#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <pthread.h>

#define BUFSIZE		4096
#define THREADS		3
#define SPACE		" "

//filename as a global variable
char 		*filename;
char		*service;
float 		g_timeout;
char		*host = "localhost";
char 		*directory = "./readers"; // directory where all newly created files will be stored
//counters
int total_readers = 0;
int total_writers = 0;
int total_rtimeout = 0;
int total_wtimeout = 0;

//mutex
pthread_mutex_t lock; 
int connectsock( char *host, char *service, char *protocol );

/*
**	Client
*/
/*      
**	Poisson interarrival times. Adapted from various sources
**      L = desired arrival rate
*/
double poissonRandomInterarrivalDelay( double L )
{
    return (log((double) 1.0 - ((double) rand())/((double) RAND_MAX)))/-L;
}
void *reader(void *t);
void *writer(void* t);

int
main( int argc, char *argv[] )
{
	//rclient [host] port rate filename directory timeout
	char		buf[BUFSIZE];		
	char		*host = "localhost";
	char 		*port;
	float 		rate;
	// char 		*filename;	
	float 		timeout; 
	int		cc;
	int csock;

	//check or arguments needed first to prevent segmentation fault!!!
	if(argc == 1){
		fprintf( stderr, "usage example: rclient [host] port rate filename directory timeout\n" );
		exit(-1);
	}
	//./client rclient [host] port rate filename directory timeout
	if(strcmp(argv[1], "rclient") == 0){
		switch( argc ) 
		{
			case    8:
				host  = argv[2];
                service = argv[3];
                rate = atof(argv[4]);
                filename = argv[5];
                directory = argv[6];
                timeout = atof(argv[7]);
				break;
			default:
				fprintf( stderr, "rclient [host] port rate filename directory timeout\n" );
				exit(-1);
		}
		//The reader client will generate threads at the specified rate. 
		pthread_t rthread_id[THREADS];
		int j;
		g_timeout = timeout;
		for(int i = 0, j = 0; i < THREADS; i++, j++){
			pthread_create(&rthread_id[i], NULL, reader, (void *) j);
			sleep(poissonRandomInterarrivalDelay(rate));
		}

		for(int i = 0; i < THREADS; i++){
			pthread_join(rthread_id[i], NULL);
		}
		close(csock);
		exit(0);
	}
	//Writer client
	//./client wclient [host] port rate filename timeout
	if(strcmp(argv[1], "wclient") == 0){
		printf("In writer");
		switch( argc ) 
		{
			case    7:
				host  = argv[2];
                service = argv[3];
                rate = atof(argv[4]);
                filename = argv[5];
                timeout = atof(argv[6]);
                //service = argv[6];
				break;
			default:
				fprintf( stderr, "wclient [host] port rate filename timeout\n" );
				exit(-1);
		}
		g_timeout = timeout;
		/*	Create the socket to the controller  */
		if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
		{
			fprintf( stderr, "Cannot connect to server.\n" );
			exit( -1 );
		}
		fflush(stdout);

		//The writer client will generate threads at the specified rate. 
		pthread_t wthread_id[THREADS];
		int j;
		for(int i = 0, j = 0; i < THREADS; i++, j++){
			pthread_create(&wthread_id[i], NULL, writer, (void *) j);
			sleep(poissonRandomInterarrivalDelay(rate));
		}

		for(int i = 0; i < THREADS; i++){
			pthread_join(wthread_id[i], NULL);
		}
		close(csock);
		exit(0);
	}
}

void *reader(void *t)
{
	int		csock;
	int		cc;
	char reader_buf[BUFSIZE];
	/*	Create the socket to the controller  */
	//Each thread will make a separate connection the specified server  
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}
	// printf( "The server is ready, please start sending to the server.\n" );
	fflush( stdout );

	//and request the specified file
	/**								
	Server responds: 	SIZE size data
	Connection is closed.
	**/
	//Each thread should have a unique ID
	int tid;
    tid = (int) t;
    //Reader sends: 		READ nameCRLF
	strcpy(reader_buf, "READ ");
	strcat(reader_buf, filename);
	printf("\n Client nubmber %d started\n", tid);
	// printf("buf  %s\n", reader_buf);
	// Send to the server the filename
	if ( write( csock, reader_buf, strlen(reader_buf) ) < 0 )
	{
		fprintf( stderr, "client write: %s\n", strerror(errno) );
		exit( -1 );
	}
	total_readers++;
	//Timeout https://www.gnu.org/software/libc/manual/html_node/Waiting-for-I_002fO.html
	fd_set set;
	struct timeval time_out;
	/* Initialize the file descriptor set. */
    FD_ZERO (&set);
    FD_SET (csock, &set);

    /* Initialize the timeout data structure. */
  	time_out.tv_sec = g_timeout;
  	time_out.tv_usec = 0;

    if(select(FD_SETSIZE, &set, NULL, NULL, &time_out) == 0){
    	printf("Timeout for reader #%d \n", tid);
    	total_rtimeout++;
    	close(csock);
    	pthread_exit(0);
    }

	//listen to server: SIZE sent
	if ( (cc = read( csock, reader_buf, 20 )) <= 0 )
    {
    	printf( "The server has gone.\n" );
    	close(csock);
    } else {
        reader_buf[cc] = '\0';
        printf( "%s\n", reader_buf );
        // strcpy(reader_buf, "");
	}
	

	//CREATE A FILE
	FILE *fp;
	char id[20];
	sprintf(id, "%d", tid);
  	strcpy(filename, "./readers/reader(");
	strcat(filename, id);
	strcat(filename, ").txt");
	printf("\n %s \n",filename);
	filename[strlen(filename)]= '\0';

	fp = fopen(filename,"w");

	// //listen to 2MB data 
	for(int i = 0; i < 512; i++){
		if ( (cc = read( csock, reader_buf, BUFSIZE)) <= 0 )
	    {
	    	printf( "The server has gone.\n" );
	    	close(csock);
	    } else {
	        reader_buf[cc] = '\0';
	        fprintf(fp, "%s", reader_buf);
	        // pthread_exit(0);
		}
	}
	
	close( csock );
	// printf("%d\n %d\n", total_rtimeout, total_readers);
	pthread_exit(NULL);
} 

void *writer(void* t){

	char 	writer_buf[BUFSIZE];
	int		csock;
	int		cc;
	
	/*	Create the socket to the controller  */
	//Each thread will make a separate connection the specified server  
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}
	// printf( "The server is ready, please start sending to the server.\n" );
	fflush( stdout );
	//and request the specified file
	/**								
	Writer sends: 		WRITE nameCRLF
	Connection is closed.
	**/
	//Each thread should have a unique ID
	int tid;
    tid = (int) t;
    //Writer sends: 		WRITE nameCRLF

	strcpy(writer_buf, "WRITE ");
	strcat(writer_buf, filename);
	printf("\n Client nubmber %d started\n", tid);
	printf("Contents of the buf are %s\n", writer_buf);
	// Send to the server the filename
	if ( write( csock, writer_buf, strlen(writer_buf) ) < 0 )
	{
		fprintf( stderr, "client write: %s\n", strerror(errno) );
		exit( -1 );
	}
	total_writers++;

	fd_set set;
	struct timeval time_out;
	/* Initialize the file descriptor set. */
    FD_ZERO (&set);
    FD_SET (csock, &set);

    /* Initialize the timeout data structure. */
  	time_out.tv_sec = g_timeout;
  	time_out.tv_usec = 0;

    if(select(FD_SETSIZE, &set, NULL, NULL, &time_out) == 0){
    	printf("Timeout for writer #%d \n", tid);
    	total_wtimeout++;
    	close(csock);
    	pthread_exit(0);
    }
	//listen to server: GO nameCRLF
	if ( (cc = read( csock, writer_buf, 20 )) <= 0 )
    {
    	printf( "The server has gone.\n" );
    	close(csock);
    } else {
        writer_buf[cc] = '\0';
        // printf( "%s\n", writer_buf );
        // strcpy(writer_buf, "");
	}

	//Где-то здесь таймаут

	//send to the server SIZE 
	strcpy(writer_buf,"SIZE 2MB ");
	if ( write( csock, writer_buf, strlen(writer_buf) ) < 0 )
	{
		fprintf( stderr, "client write: %s\n", strerror(errno) );
		exit( -1 );
	}
	char c[10];
	sprintf(c, "%d", tid);
	char buf[BUFSIZE];
	for(int i = 0; i < BUFSIZE; i++){
		buf[i] = c;
	}
	for(int j = 0; j < 512; j++){
		if ( write( csock, buf, BUFSIZE ) < 0 )
		{
			fprintf( stderr, "client write: %s\n", strerror(errno) );
			exit( -1 );
		}
	}

	//pthread_mutex_unlock(&lock);
}