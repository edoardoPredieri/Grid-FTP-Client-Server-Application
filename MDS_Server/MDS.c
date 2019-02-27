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
    int mem;
    int online;
    struct dr* next;
} dr;

typedef struct file {
    char* name;
    int size;
    char** blocks;
    struct file* next;
} file;

int num_client=0;
char* pathL = "login.txt";
char* pathD = "dr.txt";
client** clientList;


dr* removeListD(dr* l,int size){
	dr* tmp =l;
    while(tmp!=NULL){
		if(tmp->online){
			tmp->mem+=size;
		}
        tmp=tmp->next;
    }
    return l;

    //mandare mess DRs
}

int getSize(file* l, char* n){
	file* tmp =l;
    while(tmp!=NULL){
        if(strcmp(tmp->name, n)==0){
            return tmp->size;
        }
        tmp=tmp->next;
    }
    return 0;
}

// send key of client to DRs
void sendKey(dr* l, char* k){
	if(l==NULL){
		return;
	}

	if(l->online){
		int ret;

		char buf[1024];
		size_t buf_len = sizeof(buf);
		size_t msg_len;

		// variables for handling a socket
		int socket_desc;
		struct sockaddr_in server_addr = {0};

		// create a socket
		socket_desc = socket(AF_INET, SOCK_STREAM, 0);
		ERROR_HELPER(socket_desc, "Could not create socket");

		// set up parameters for the connection
		server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
		server_addr.sin_family      = AF_INET;
		server_addr.sin_port        = htons(l->port);

		// initiate a connection on the socket
		ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
		if(ret==-1){
			ERROR_HELPER(ret, "Cannot send the Key");
		}
		else {
			printf("Send key to DR n: %d\n",l->port);
			sprintf(buf,"Save %s\n",k);
			while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
				if (errno == EINTR)
					continue;
					ERROR_HELPER(-1, "Cannot write to the socket");
			}

			while ((ret = recv(socket_desc, buf, buf_len, 0)) < 0){
				if (errno == EINTR)
					continue;
					ERROR_HELPER(-1, "Cannot read from socket");
			}
			printf("DR response %s\n",buf);
			ret = close(socket_desc);
			ERROR_HELPER(ret, "Cannot close socket");
		}
	}
	sendKey(l->next, k);
}

void printFile(file* l){
	if(l==NULL)
		return;
	int i=0;
	while(l->blocks[i]!=NULL){
		printf("name = %s    size:%d   block[%d]=%s\n",l->name, l->size,i, l->blocks[i]);
		i++;
	}
	printFile(l->next);
}

void printDR(dr* l){
	if(l==NULL)
		return;
	printf("port = %d    online:%d      mem=%d\n",l->port, l->online, l->mem);
	printDR(l->next);
}

int onlineDR(dr*l){
    if (l==NULL){
        return 0;
    }
    if (l->online==0){
        return 0 + onlineDR(l->next);
    }
    return 1 + onlineDR(l->next);
}

// insert elem in tail FILE
file* insTailFILE(file* l, char* name, int size, char** b){
    file* tmp = l;

    while (tmp->next!=NULL){
        tmp=tmp->next;
    }
    tmp->next=(file*)malloc(sizeof(file));
    tmp=tmp->next;
    tmp->name=name;
    tmp->size=size;
    tmp->blocks=b;
    tmp->next=NULL;
    return l;
}

// insert elem in tail
dr* insTail(dr* l, int pos, int port, int mem, int online){
    dr* tmp = l;

    while (tmp->next!=NULL){
        tmp=tmp->next;
    }
    tmp->next=(dr*)malloc(sizeof(dr));
    tmp=tmp->next;
    tmp->pos=pos;
    tmp->port=port;
    tmp->mem=mem;
    tmp->online=online;
    tmp->next=NULL;
    return l;
}

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

// converts to String from Int
char* itoa(int i){
    char* ret =malloc(sizeof(char)*i);
    sprintf(ret,"%d",i);
    return ret;
}

// controll the online DRs
dr* init(){
	FILE* f = fopen(pathD, "r");
    int i=0;
    int j=0;

	dr* drList=(dr*)malloc(sizeof(dr*));
	drList->next=NULL;

	while(1){
        char* tmp=(char*)calloc(0,sizeof(char)*3);

        if(fscanf(f,"%s",tmp)==EOF){
            break;
        }

        int pos=i;
        int port=atoi(tmp);
        int online;
        int mem=1000;

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
            online=0;
		}

		else {
			online=1;
			ret = close(socket_desc);
			ERROR_HELPER(ret, "Cannot close socket");
		}

		drList=insTail(drList, pos, port, mem, online);
		free(tmp);
    }
    fclose(f);
    return drList->next;
}

dr* check(dr** drList){
	dr* tmp=*drList;

	while(tmp!=NULL){
		int ret;
		int online;

		// variables for handling a socket
		int socket_desc;
		struct sockaddr_in server_addr = {0};

		// create a socket
		socket_desc = socket(AF_INET, SOCK_STREAM, 0);
		ERROR_HELPER(socket_desc, "Could not create socket");

		// set up parameters for the connection
		server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
		server_addr.sin_family      = AF_INET;
		server_addr.sin_port        = htons(tmp->port);

		// initiate a connection on the socket
		ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
		if(ret==-1){
            online=0;
		}
		else {
			online=1;
			ret = close(socket_desc);
			ERROR_HELPER(ret, "Cannot close socket");
		}
		tmp->online=online;
		tmp=tmp->next;
	}
	return *drList;
}

char* getDR(dr** drList){
    *drList=check(drList);

    int online=onlineDR(*drList);
    char* s=(char*)malloc(sizeof(char)*5*online);
    int i=0;
    for(i;i<5*online;i++){
		s[i]=0;
	}
    dr* tmp=*drList;

    while(tmp!=NULL){
		if(tmp->online){
			strcat(s, itoa(tmp->port));
            strcat(s, " ");
		}
		tmp=tmp->next;
	}
    return s;
}

int* Put(int socket_desc, char* k, dr** drList,int size){
	char buf[1024];
    size_t buf_len = sizeof(buf);
    size_t msg_len;

    int sizeBlock = size / onlineDR(*drList);
    int rest=size%onlineDR(*drList);

	int* l=(int*)malloc(sizeof(int)*(size+rest));
    dr* tmp=check(drList);

    sendKey(*drList, k);

    int ret;

    int i=0;
    int j=0;
    while(tmp->next!=NULL){
		if(tmp->mem - sizeBlock -rest > 0){
			if(tmp->online){
				for(i;i<sizeBlock+j;i++){
					l[i]=tmp->port;
				}
				if(rest > 0){
					l[i]=tmp->port;
					i++;
				}
				j=i;
				if(rest>0){
					tmp->mem = tmp->mem - sizeBlock- rest;
				}
				else{
					tmp->mem = tmp->mem - sizeBlock;
				}
				rest--;
			}
		}
		else{
			sprintf(buf,"MEMORY FULL\n");
			while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
				if (errno == EINTR)
					continue;
					ERROR_HELPER(-1, "Cannot write to the socket");
			}
			free(l);
			return NULL;
		}
		tmp=tmp->next;
    }

	int first=0;
	char* s=(char*)malloc(sizeof(char)*100);

    for(i=0;i<100;i++){
		s[i]=0;
	}

	for (i=0;i<size;i++){
		if(l[i] != first){
			if(i!=0){
				strcat(s, itoa(i-1));
				strcat(s, ",");
			}
			strcat(s, itoa(l[i]));
			strcat(s, " ");
			if(i!=size-1){
				strcat(s, itoa(i));
				strcat(s, " ");
			}
		first=l[i];
		}
	}
	strcat(s, itoa(i-1));

    sprintf(buf,"%s",s);
	while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
		if (errno == EINTR)
			continue;
            ERROR_HELPER(-1, "Cannot write to the socket");
	}

    free(s);
    return l;
}

char* Get(file* fileList, char* name){
	file* tmp =fileList;
	char* s;
	tmp=tmp->next;

	printFile(tmp);

	int i=0,j=0;

	while(tmp!=NULL){
		if(strcmp (tmp->name, name)==0){
			i=0;
			while(tmp->blocks[i]!=NULL){
				i++;
			}
			char* s=(char*)malloc(sizeof(char)*(i+1));
			for(j=0;j<(i+1);j++){
				s[j]=0;
			}
			i=0;
			while(tmp->blocks[i]!=NULL){
				strcat(s,tmp->blocks[i]);
				strcat(s," ");
				i++;
			}
			return s;
		}
		tmp=tmp->next;
	}
	return NULL;
}

file* RemoveFile(file* fileList, char* name){
	file* tmp =fileList;
	tmp=tmp->next;

	if(strcmp(tmp->name, name)==0){
		if(tmp->next!=NULL)
			return fileList->next;
		else
			return (file*)malloc(sizeof(file));
	}

    while(tmp!=NULL){
        if(strcmp(tmp->next->name, name)==0){
			if(tmp->next->next!=NULL){
				tmp->next=tmp->next->next;
			}
			else{
				tmp->next=NULL;
			}
            break;
        }
        tmp=tmp->next;
    }
	return fileList;
}

int verify_client(int id, int socket_desc, struct client* tmpClient){
    FILE* f = fopen(pathL, "r");
    int ret;

    char* key =itoa(id^12139456);

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
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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
                    sprintf(buf,"K %s", key);
                    while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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
                    sprintf(buf, "NOK you can retry again\n");
                    while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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
                        sprintf(buf,"%s", key);
                        while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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
                        sprintf(buf, "NOK, Bye\n");
                        while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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
    while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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
    while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
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

        dr* drList = init();
        printDR(drList);

        file* fileList = (file*)malloc(sizeof(file));
        fileList->next=NULL;

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

            // if I receive GetDR command
            if(strncmp (buf, "GetDR",5 )==0){
				char* s=getDR(&drList);
                sprintf(buf,"%s",s);
                printDR(drList);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                if (errno == EINTR)
                    continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                free(s);
            }

            // if I receive Put command
            else if(strncmp (buf, "Put",3 )==0){
				char buf2[recv_bytes];
				strncpy(buf2, buf, recv_bytes);

                char** query=str_split(buf2,' ');
                char* name=query[1];
                int size=atoi(query[2]);

                int* l=Put(socket_desc, tmpClient->key, &drList, size);

				fileList=insTailFILE(fileList, name, size, str_split(getDR(&drList),' '));

				printDR(drList);

                free(query);
            }

            // if I receive Get command
            else if(strncmp (buf, "Get",3 )==0){
				char buf2[recv_bytes];
				strncpy(buf2, buf, recv_bytes);

                char** query=str_split(buf2,' ');
                char* name=query[1];

                char* s=Get(fileList, name);
                sprintf(buf,"%s",s);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                if (errno == EINTR)
                    continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                free(query);
			}

			else if(strncmp (buf, "Remove",6 )==0){
				char buf2[1024];
				strncpy(buf2, buf, recv_bytes);

				char** query=str_split(buf2,' ');
                char* name=query[1];

                int size=getSize(fileList->next,name);

                fileList=RemoveFile(fileList, name);

				drList=removeListD(drList, size/onlineDR(drList));

                sprintf(buf,"OK");
                printDR(drList);
                while ((ret = send(socket_desc, buf, sizeof(buf), 0)) < 0){
                if (errno == EINTR)
                    continue;
                    ERROR_HELPER(-1, "Cannot write to the socket");
                }
                free(query);
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

    clientList = (client**)malloc(sizeof(client*)*num_client);

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
