#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>  // htons()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>

#include "common.h"

char* path="keys.txt";
int keySize=8;

typedef struct handler_args_s{
    int socket_desc;
    struct sockaddr_in *client_addr;
} handler_args_t;

// split command
char** str_split(char* a_str, const char a_delim){
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    // count how many elements will be extracted
    while (*tmp){
        if (a_delim == *tmp){
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    // Add space for trailing token
    count += last_comma < (a_str + strlen(a_str) - 1);

    // Add space for terminating null string so caller knows where the list of returned strings ends
    count++;

    result = malloc(sizeof(char*) * count);

    if (result){
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token){
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

int verifyKey(char* k){
	FILE* f = fopen(path, "r");
	while(1){
        char* tmp=malloc(sizeof(char)*keySize);

        if(fscanf(f,"%s",tmp)==EOF){
            break;
        }
        if(atoi(tmp)==atoi(k)){
            fclose(f);
			return 1;
		}
	}
    fclose(f);
    return 0;
}

void *connection_handler(void *arg){
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
    uint16_t client_port = ntohs(client_addr->sin_port); // port number is an unsigned short

    // echo loop
    while (1){
        // read message from client
        while ((recv_bytes = recv(socket_desc, buf, buf_len-1, 0)) < 0){
            if (errno == EINTR)
                continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }

        buf[recv_bytes]='\0';

        // check whether I have just been told to quit...
        if (recv_bytes == 0) break;
        if (recv_bytes == quit_command_len && !memcmp(buf, quit_command, quit_command_len))
            break;

        // if I receive Save command
        if(strncmp (buf, "Save",4 )==0){
                char** query=str_split(buf,' ');
                char* key=query[1];

                if(!verifyKey(key)){
                    FILE* f=fopen(path, "a");
                    fprintf(f,"%s",key);
                    fclose(f);
                }

                sprintf(buf, "OK");
                msg_len = strlen(buf);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                    if (errno == EINTR)
                    continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                break;
        }

        else if(strncmp (buf, "Put",3 )==0){
            char** query=str_split(buf,'$');
            char* block=query[1];
            char* name=query[2];
            char* key=query[3];



            printf("Recevive block=%s name=%s key=%s\n",block,name,key);
            if(verifyKey(key)){
                strcat(key,name);

                FILE* f=fopen(key,"w");
                fputs(block,f);
                fclose(f);

                sprintf(buf, "Block received");
                msg_len = strlen(buf);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                    if (errno == EINTR)
                        continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                break;
            }
            else{
                sprintf(buf, "Error: Key not valid");
                msg_len = strlen(buf);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                    if (errno == EINTR)
                        continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                break;
            }
        }

        else if(strncmp (buf, "Get",3 )==0){
            char** query=str_split(buf,'$');
            char* name=query[1];
            char* key=query[2];


            //---------------------- if key in mykey

            strcat(key,name);
            FILE* f=fopen(key,"r");

            int n=1,j=0;
            char* fileBuf=(char*)malloc(sizeof(char)*n);

            while(1){
                if(j>=n){
                    n=n*2;
                    fileBuf=(char*)realloc(fileBuf, n*sizeof(char));
                }
                char tmp[1];
                if(fscanf(f,"%c",tmp)==EOF){
                    break;
                }
                fileBuf[j]=tmp[0];
                j++;
            }

            fclose(f);

            sprintf(buf, "%s",fileBuf);
            msg_len = strlen(buf);
            while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                if (errno == EINTR)
                    continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
            }
            break;
        }

        else if(strncmp (buf, "Remove",6 )==0){
            char** query=str_split(buf,' ');
            char* name=query[1];

            printf("name=%s\n",name);

            int status=remove(name);

            if(status==0){
                sprintf(buf, "OK");
                msg_len = strlen(buf);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                    if (errno == EINTR)
                        continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                break;
            }
            else{
                sprintf(buf, "NOK");
                msg_len = strlen(buf);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                    if (errno == EINTR)
                        continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                break;
            }
        }


        else{
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

    free(args->client_addr); // do not forget to free this buffer!
    free(args);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
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

    exit(EXIT_SUCCESS); // this will never be executed
}
