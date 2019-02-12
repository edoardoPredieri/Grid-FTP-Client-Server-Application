#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>

#include "common.h"

typedef struct handler_args_s {
    int socket_desc;
    struct sockaddr_in *client_addr;
} handler_args_t;

typedef struct client {
    int pos;
    int id;
    char* key;
} client;

typedef struct dr {
    int pos;
    //int ip;
    int port;
    int* mem;
} dr;

int num_client=0;
int num_dr=0;
char* pathL = "login.txt";
char* pathD = "dr.txt";
client** clientList;
dr** drList;


// controll the online DRs
void init(){
	FILE* f = fopen(pathD, "r");
	while(1){
        char* tmp=malloc(sizeof(char)*3);

        if(fscanf(f,"%s",tmp)==EOF){
            break;
        }
        
        // try connection
        int ret;

		// variables for handling a socket
		int socket_desc;
		struct sockaddr_in server_addr = {0}; 

		// create a socket
		socket_desc = socket(AF_INET, SOCK_STREAM, 0);
		ERROR_HELPER(socket_desc, "Could not create socket");

		// set up parameters for the connection
		server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
		server_addr.sin_family      = AF_INET;
		server_addr.sin_port        = htons(atoi(tmp));

		// initiate a connection on the socket
		ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
		if(ret==-1){
			printf("DR offline\n");
			continue;
		}
		
		if (DEBUG){
			fprintf(stderr, "DR n:%d online\n",num_dr);
			struct dr* drTmp = malloc(sizeof(dr));
			drTmp->pos=num_dr;
			//drTmp->ip=atoi(tmp);
			drTmp->port=atoi(tmp); 
			drTmp->mem=(int*)malloc(sizeof(int)*10);

			drList=(dr**)realloc(drList, (num_dr+1)*sizeof(dr*));
			drList[num_dr]=drTmp;
			num_dr++;
			
		}
		
		ret = close(socket_desc);
		ERROR_HELPER(ret, "Cannot close socket");
	
        free(tmp);
    }
    fclose(f);
}

// create a random string
char* randstring(size_t length) {
    static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";        
    char *randomString = NULL;

    if (length) {
        randomString = malloc(sizeof(char) * (length +1));

        if (randomString) {   
            int n;         
            for (n = 0;n < length;n++) {            
                int key = rand() % (int)(sizeof(charset) -1);
                randomString[n] = charset[key];
            }

            randomString[length] = '\0';
        }
    }

    return randomString;
}

int verify_client(int id, int socket_desc, struct client* tmpClient){
    FILE* f = fopen(pathL, "r");
    int ret;
    char* key = randstring(8);
    
    tmpClient->id=id;
    tmpClient->key=key;
        
 
    while(1){
        char* tmp=malloc(sizeof(char)*3);

        if(fscanf(f,"%s",tmp)==EOF){
            break;
        }
        
        // verify if the client is already registered
        if(atoi(tmp)==id){
            
                char* passw = malloc(sizeof(char)*20);
                fscanf(f,"%s",passw);
                
                // send login request to client
                char buf[1024];
                size_t buf_len = sizeof(buf);
                size_t msg_len;
                
                sprintf(buf, "Welcome Client n:%d, to login type: Auth <Userid> <Password>\n", id);
                while ((ret = send(socket_desc, buf, 61, 0)) < 0){
                    if (errno == EINTR)
                        continue;
                ERROR_HELPER(-1, "Cannot write to the socket (password request)");
                }
                
                // receive password from client
                while ( (msg_len = recv(socket_desc, buf, buf_len - 1, 0)) < 0 ) {
                    if (errno == EINTR) continue;
                        ERROR_HELPER(-1, "Cannot read Password from Client");
                }
                
                buf[msg_len] = '\0';
                
                // forge the Auth query
                char* query =(char*) malloc(sizeof(char)*5000);
                strcat(query,"Auth ");
                strcat(query,tmp);
                strcat(query," ");
                strcat(query,passw);
                
                // compare the quety with login
                if (strcmp(query, buf)==0){
                    
                    strcpy(buf, key);
                    while ((ret = send(socket_desc, buf, 8, 0)) < 0){
                        if (errno == EINTR)
                            continue;
                        ERROR_HELPER(-1, "Cannot write to the socket (correct password)");
                    }
                    
                    free(query);
                    free(tmp);
                    fclose(f);
                    return 1;
                }
                
                else{
                    sprintf(buf, "NOK\n");
                    while ((ret = send(socket_desc, buf, 4, 0)) < 0){
                        if (errno == EINTR)
                            continue;
                        ERROR_HELPER(-1, "Cannot write to the socket (wrong password)");
                    }
                    
                    // receive password from client 2th
                    while ( (msg_len = recv(socket_desc, buf, buf_len - 1, 0)) < 0 ) {
                        if (errno == EINTR) continue;
                        ERROR_HELPER(-1, "Cannot read Password from Client");
                    }
                
                    buf[msg_len] = '\0';
        
                    char* query =(char*) malloc(sizeof(char)*5000);
                    strcat(query,"Auth ");
                    strcat(query,tmp);
                    strcat(query," ");
                    strcat(query,passw);
                
                    if (strcmp(query, buf)==0){
                        strcpy(buf, key);
                        while ((ret = send(socket_desc, buf, 8, 0)) < 0){
                            if (errno == EINTR)
                                continue;
                            ERROR_HELPER(-1, "Cannot write to the socket (correct password)");
                        }
                        free(query);
                        free(tmp);
                        fclose(f);
                        return 1;
                    }
                
                    else{
                        sprintf(buf, "NOK\n");
                        while ((ret = send(socket_desc, buf, 4, 0)) < 0){
                            if (errno == EINTR)
                                continue;
                            ERROR_HELPER(-1, "Cannot write to the socket (wrong password)");
                        }
                        free(query);
                        free(tmp);
                        fclose(f);
                        
                        return 0;
                    }
                }
        }
        free(tmp);
    }
    
    fclose(f);
    
    f = fopen(pathL, "a");
    
    // send register request to client
    char buf[1024];
    size_t buf_len = sizeof(buf);
    size_t msg_len;
               
    sprintf(buf, "Welcome Client n:%d, to register type: Auth <Userid> <Password>\n", id);
    while ((ret = send(socket_desc, buf, 64, 0)) < 0){
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot write to the socket (registration request)");
    }
        
    // receive password from client 2th
    while ( (msg_len = recv(socket_desc, buf, buf_len - 1, 0)) < 0 ) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read Password from Client");
    }
        
    buf[msg_len] = '\0';
        
    char* query =(char*) malloc(sizeof(char)*5000);
    strcat(query,"id");
    strcat(query," ");
    strcat(query,buf);
                
    // write on file
    fprintf(f,"%s",query);
    
    sprintf(buf, "OK\n");
    while ((ret = send(socket_desc, buf, 3, 0)) < 0){
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot write to the socket (registration request)");
    }
    
    fclose(f);
    free(query);
	return 1;
}

void *connection_handler(void *arg){
    
    clientList=(client**)realloc(clientList, (num_client+1)*sizeof(client*));
    
    struct client* tmpClient =(client*)malloc(sizeof(client));
    tmpClient->pos=num_client;
    clientList[num_client]=tmpClient;
    
    num_client++;
    
    handler_args_t *args = (handler_args_t *)arg;

    int socket_desc = args->socket_desc;
    struct sockaddr_in *client_addr = args->client_addr;

    int ret, recv_bytes;

    char buf[1024];
    size_t buf_len = sizeof(buf);
    size_t msg_len;

    char *quit_command = SERVER_COMMAND;
    size_t quit_command_len = strlen(quit_command);

    // parse client IP address and port
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN);
    uint16_t client_port = ntohs(client_addr->sin_port);

    // send welcome message
    sprintf(buf, "Welcome to Grid FTP Client-Server Application made by Edoardo Predieri. I will stop if you send me %s, Press 'Send' \n", quit_command);
    msg_len = strlen(buf);
    while ((ret = send(socket_desc, buf, msg_len, 0)) < 0){
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot write to the socket");
    }
    
    // receive the ID from Client
    while ( (msg_len = recv(socket_desc, buf, buf_len - 1, 0)) < 0 ) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read ID from Client");
    }
    
    buf[msg_len] = '\0';
    
    // save and verify Client ID
    int client_id = atoi(buf);
    printf("client ID = %d\n", client_id);

	if(verify_client(client_id, socket_desc, tmpClient)){
        // echo loop
        while (1){
            // read message from client
            while ((recv_bytes = recv(socket_desc, buf, buf_len, 0)) < 0){
                if (errno == EINTR)
                    continue;
                ERROR_HELPER(-1, "Cannot read from socket");
            }

            // check whether I have just been told to quit...
            if (recv_bytes == 0) break;
            if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len))
                break;

            // ... or if I have to send the message back
            while ((ret = send(socket_desc, buf, recv_bytes, 0)) < 0){
                if (errno == EINTR)
                    continue;
                ERROR_HELPER(-1, "Cannot write to the socket");
            }
        }
    }
    // close socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket for incoming connection");

    if (DEBUG)
        fprintf(stderr, "Thread created to handle the request has completed.\n");


    free(tmpClient);
    clientList[num_client]=NULL;
    num_client--;

    free(args->client_addr);
    free(args);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
	
	init();
	
	printf("END setup phase\n");
    
    clientList = (client**)malloc(sizeof(client*)*num_client);
    drList = (dr**)malloc(sizeof(dr*)*num_dr);
    
    int ret;

    int socket_desc, client_desc;

    // some fields are required to be filled with 0
    struct sockaddr_in server_addr = {0};

    int sockaddr_len = sizeof(struct sockaddr_in); // we will reuse it for accept()

    // initialize socket for listening
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); // don't forget about network byte order!

    /* We enable SO_REUSEADDR to quickly restart our server after a crash:
     * for more details, read about the TIME_WAIT state in the TCP protocol */
    int reuseaddr_opt = 1;
    ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

    // bind address to socket
    ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Cannot bind address to socket");

    // start listening
    ret = listen(socket_desc, MAX_CONN_QUEUE);
    ERROR_HELPER(ret, "Cannot listen on socket");

    // we allocate client_addr dynamically and initialize it to zero
    struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));

    // loop to manage incoming connections spawning handler threads
    while (1){
        // accept incoming connection
        client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
        if (client_desc == -1 && errno == EINTR)
            continue; // check for interruption by signals
        ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");

        if (DEBUG)
            fprintf(stderr, "Incoming connection accepted...\n");

        pthread_t thread;

        // put arguments for the new thread into a buffer
        handler_args_t *thread_args = malloc(sizeof(handler_args_t));
        thread_args->socket_desc = client_desc;
        thread_args->client_addr = client_addr;
        
        ret = pthread_create(&thread, NULL, connection_handler, (void *)thread_args);
        PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

        if (DEBUG)
            fprintf(stderr, "New thread created to handle the request!\n");

        ret = pthread_detach(thread); // I won't phtread_join() on this thread
        PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");

        // we can't just reset fields: we need a new buffer for client_addr!
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }

    exit(EXIT_SUCCESS);
}
