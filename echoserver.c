#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>

#define	QLEN			5
#define	BUFSIZE			4096
#define SPACE			" "
//mutex
pthread_mutex_t lock; 

int passivesock( char *service, char *protocol, int qlen, int *rport );

void *rp_thread( void *s );
void *wp_thread( void *s );
void reader( int ssock, char *filename );
void writer( int ssock, char *filename );
/*
*/

int main( int argc, char *argv[] )
{
	char			*service;
	struct sockaddr_in	fsin;
	int			alen;
	int			msock;
	int			ssock;
	int			rport = 0;
	char		*pref;
	
	switch (argc) 
	{
		case	2:
			// No argument for port? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	3:
			pref = argv[1];
			service = argv[2];
			break;
		default:
			fprintf( stderr, "usage: server [rp/wp] [port]\n" );
			exit(-1);
	}
	msock = passivesock( service, "tcp", QLEN, &rport );

	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}
	
	for (;;)
	{
		int	ssock;
		pthread_t	thr;

		alen = sizeof(fsin);
		ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
		if (ssock < 0)
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			break;
		}

		printf( "A client has arrived for echoes.\n" );
		fflush( stdout );
		if(strcmp(pref,"rp") == 0){
			pthread_create( &thr, NULL, rp_thread, (void *) ssock );
		} else if(strcmp(pref, "wp") == 0){
			pthread_create( &thr, NULL, wp_thread, (void *) ssock );
		}
		// pthread_create( &thr, NULL, rp_thread, (void *) ssock );

	}
	pthread_exit(0);
}

//Thread for reader preference
sem_t resource;
sem_t rmutex;
int readcount = 0;
void *rp_thread( void *s )
{
	char buf[BUFSIZE];
	char file_buf[BUFSIZE];
	int cc;
	char filename[30], first[30];
	int ssock = (int) s;

	//Server responds: 	SIZE size data
	printf("This is rp thread! ");
	if((cc = read(ssock, buf, BUFSIZE)) <= 0){
		printf("The client has gone.\n");
		close(ssock);
	} else {
		printf("Client says: %s\n", buf);		
	}    
	//Tokenize buf to determine READ or WRITE and name
	char *token;
    token = strtok(buf, SPACE);
    strcpy(first,token); 
    token = strtok(NULL, SPACE);
    strcpy(filename,token);
	if(strcmp(first, "READ") == 0){
		//wait rmutex
		sem_wait(&rmutex);
		reader(ssock,filename);
		sem_post(&rmutex);
	} else if(strcmp(first, "WRITE") == 0){
		//wait resource
		sem_wait(&resource);
		writer(ssock, filename);
		sem_post(&resource);
	}

	pthread_exit(0);
}

//Thread for writer preference
int readcount2 = 0, writecount = 0;
sem_t r_mutex, wmutex, readTry, resource2;

void *wp_thread( void *s )
{
	char buf[BUFSIZE];
	char file_buf[BUFSIZE];
	int cc;
	char filename[30], first[30];
	int ssock = (int) s;

	//Server responds: 	SIZE size data
	printf("This is wp thread! ");
	if((cc = read(ssock, buf, BUFSIZE)) <= 0){
		printf("The client has gone.\n");
		close(ssock);
	} else {
		printf("Client says: %s\n", buf);		
	}    
	//Tokenize buf to determine READ or WRITE and name
	char *token;
    token = strtok(buf, SPACE);
    strcpy(first,token); 
    token = strtok(NULL, SPACE);
    strcpy(filename,token);
	if(strcmp(first, "READ") == 0){
		sem_wait(&readTry);
		sem_wait(&rmutex);
		readcount2++;
		if(readcount2 == 1){
			sem_wait(&resource2);
		}
		sem_post(&rmutex);
		sem_post(&readTry);
		reader(ssock,filename);
		sem_wait(&rmutex);
		readcount2--;
		if(readcount2 == 0){
			sem_post(&resource2);
		}
		sem_post(&rmutex);
	} else if(strcmp(first, "WRITE") == 0){
		sem_wait(&wmutex);
		writecount++;
		if(writecount == 1){
			sem_post(&readTry);
		}
		sem_post(&wmutex);
		writer(ssock, filename);
		sem_wait(&wmutex);
		writecount--;
		if(writecount == 0){
			sem_post(&readTry);
		}
		sem_post(&wmutex);
	}
		
	pthread_exit(0);
}


void reader( int ssock, char *filename){

	char buf[BUFSIZE];
	char *file_buf = malloc(sizeof(char)*(3*1024*1024));;
	int cc;

	// printf("Filename is %s\n",filename);
	//open file 
	FILE *fp;
	char str[BUFSIZE];
	fp = fopen("asd.txt", "r");
	//get size and data from file
	int size = 0;
	while(fgets(str, BUFSIZE, fp) != NULL){
		// puts(str);
		size += strlen(str);
		strcat(file_buf, str);
		// printf("Temp Buf is %s\n", temp_buf);
		//cc = write(ssock, str, strlen(str));
	}
	fclose(fp);
	//convert size to string
	char size_string[20];
	sprintf(size_string, "%d", size);
	//SERVER send: SIZE size data
	char *temp_buf = malloc(sizeof(char)*100);
	strcpy(temp_buf, "SIZE ");
	strcat(temp_buf, size_string);
	printf("%s\n", temp_buf);
	if ( write( ssock, temp_buf, strlen(temp_buf) ) < 0 )
	{
		/* Smth went wrong */
		printf("Write failed");
		pthread_exit(0);
	} 
	printf("Written size");

	//Write to client
	for(int i = 0; i < 512; i++){
		if ( write( ssock, file_buf, BUFSIZE) < 0 )
		{
			/* Smth went wrong */
			printf("Write failed");
			pthread_exit(0);
		} 
	}
	pthread_exit(0);

}
void writer(int sock, char *filename){
	sem_wait(&resource2);
	char buf[BUFSIZE];
	char *file_buf = malloc(sizeof(char)*(3*1024*1024));;
	int cc;

	// printf("Filename is %s\n",filename);
	//Server responds: 	GO nameCRLF
	strcpy(buf, "GO ");
	strcat(buf, filename);
	strcat(buf, "\r\n");
	if ( write( sock, buf, BUFSIZE) < 0 )
	{	/* Smth went wrong */
		printf("Write failed");
		pthread_exit(0);
	} 
	//listen to server: SIZE sent
	if ( (cc = read( sock, buf, 20 )) <= 0 )
    {
    	printf( "The server has gone.\n" );
    	close(sock);
    } else {
        buf[cc] = '\0';
        printf( "%s\n", buf );
        // strcpy(reader_buf, "");
	}
	pthread_mutex_lock(&lock);
	//open file to write
	FILE *fp;
	// char str[BUFSIZE];
	fp = fopen(filename, "wr");
	// //listen to 2MB data 
	for(int i = 0; i < 512; i++){
		if ( (cc = read( sock, buf, BUFSIZE)) <= 0 )
	    {
	    	printf( "Client problems\n" );
	    	close(sock);
	    } else {
	        buf[cc] = '\0';
	        fprintf(fp, "%s", buf);
	        // pthread_exit(0);
		}
	}
	pthread_mutex_unlock(&lock);
	sem_post(&resource2);
	pthread_exit(0);

}