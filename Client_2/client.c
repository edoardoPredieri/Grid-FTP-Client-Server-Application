#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>  // htons() and inet_addr()
#include <netinet/in.h> // struct sockaddr_in
#include <sys/socket.h>
#include <assert.h>

#include "common.h"

// receive blocks from DRs
char* getBlock(int DR, char* k, char* n, int size, int sizeq, int id){
    int rest=size%sizeq;
    int sizeTmp=(int) size/sizeq;
    int sizeFin=0;

    if((id+1) <= rest){
        sizeFin=sizeTmp+1;
    }
    else{
        sizeFin=sizeTmp;
    }

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
    server_addr.sin_port        = htons(DR);

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    if(ret==-1){
        ERROR_HELPER(ret, "DR offline");
    }
    else {
        sprintf(buf,"Get $%s$%s",n,k);
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

        ret = close(socket_desc);
        ERROR_HELPER(ret, "Cannot close socket");
        
        buf[sizeFin]='\0';

        char* sret=(char*)malloc(sizeof(char)*sizeFin); 
        sret=strcpy(sret, buf);

        return sret;
    }
    return NULL;
}

// send blocks to DRs
void sendBlock(int DR, char* k, char* n, char* totFile, int start, int stop){
    char* b=(char*)malloc(sizeof(char)*(stop-start));

    int i=0;
    int j=start;
    for(i=0;i<=(stop-start);i++){
        b[i]=totFile[j];
        j++;
    }
    b[i]='\0';

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
    server_addr.sin_port        = htons(DR);

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    if(ret==-1){
        ERROR_HELPER(ret, "DR offline");
        free(b);
        return;
    }
    else {
        sprintf(buf,"Put $%s$%s$%s",b,n,k);
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
    free(b);
}

// save the size of files
int saveSize(char* name, int size){
    FILE* f = fopen("fileSize.txt", "r");

    while(1){
        char* tmp=malloc(sizeof(char)*20);

        if(fscanf(f,"%s",tmp)==EOF){
            break;
        }
        if(strncmp (tmp, name, strlen(name))==0){
            fclose(f);
            free(tmp);
            return 1;
        }
    }
    fclose(f);

    f = fopen("fileSize.txt", "a");
    fprintf(f,"%s ",name);
    fprintf(f,"%d\n",size);
    fclose(f);

    return 1;
}

int getSize(char* n){
    FILE* f = fopen("fileSize.txt", "r");

    while(1){
        char* tmp=malloc(sizeof(char)*20);

        if(fscanf(f,"%s",tmp)==EOF){
            break;
        }
        if(strncmp (tmp, n, strlen(n))==0){
            int tmpI;
            fscanf(f,"%d",&tmpI);
            fclose(f);
            free(tmp);
            return tmpI;
        }

    }
    fclose(f);
    return 0;
}

// split command
char** str_split(char* a_str, const char a_delim, int* size){
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

    *size=count;

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

// understand the command that the user has made
int getFlag(char* buf, int* s, char** n){
    int ret=0;
    int sizeq;

	if(strncmp (buf, "GetDR",5 )==0){
		ret=1;
	}

	else if(strncmp (buf, "Put",3 )==0){
        char** query=str_split(buf,' ',&sizeq);
        char* name=query[1];
        int size=atoi(query[2]);

        if(!saveSize(name,size)){
            printf("Error during save size of file\n");
        }
        *n=name;
        *s=size;
		ret=2;
	}

	else if(strncmp (buf, "Get",3 )==0){
        char** query=str_split(buf,' ',&sizeq);
        char* name=query[1];
        *n=name;
		ret=3;
	}

    else if(strncmp (buf, "Auth",4 )==0){
		ret=4;
	}

	return ret;
}

int main(int argc, char* argv[]){
    int ret;

    // variables for handling a socket
    int socket_desc;
    struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

    // create a socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_HELPER(socket_desc, "Could not create socket");

    // set up parameters for the connection
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT);

    // initiate a connection on the socket
    ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    ERROR_HELPER(ret, "Could not create connection");

    if (DEBUG) fprintf(stderr, "Connection established!\n");

    char buf[1024];
    size_t buf_len = sizeof(buf);
    size_t msg_len;

    // display welcome message from server
    while ( (msg_len = recv(socket_desc, buf, buf_len - 1, 0)) < 0 ) {
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Cannot read from socket");
    }

    buf[msg_len] = '\0';
    printf("%s", buf);

    // send ID to Server for eventually
    sprintf(buf, "%d\n", ID);
    msg_len = strlen(buf);
    while ((ret = send(socket_desc, buf, msg_len, 0)) < 0){
        if (errno == EINTR)
            continue;
        ERROR_HELPER(-1, "Cannot send ID");
    }

    char* key;
    int flag=0;
    int* onlineDR;
    int size;
    char* name;

    // main loop
    while (1) {
        char* quit_command = SERVER_COMMAND;
        size_t quit_command_len = strlen(quit_command);

        printf("> ");

        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }

        msg_len = strlen(buf);
        buf[--msg_len] = '\0'; // remove '\n' from the end of the message

        // send message to server
        while ( (ret = send(socket_desc, buf, msg_len, 0)) < 0) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot write to socket");
        }

        /* After a quit command we won't receive any more data from
         * the server, thus we must exit the main loop. */
        if (msg_len == quit_command_len && !memcmp(buf, quit_command, quit_command_len)) break;


        flag=getFlag(buf, &size, &name);


        // read message from server
        while ( (msg_len = recv(socket_desc, buf, buf_len, 0)) < 0 ) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }

        printf("Server response: %s\n", buf); // no need to insert '\0'

        // PUT command
        if(flag==2){
            int h=0, i=0;
            int sizeq;
            int sizeq2;

            char** query=str_split(buf,',',&sizeq);
            char** q;

            int* DR=(int*)malloc(sizeof(int)*sizeq);
            int* start=(int*)malloc(sizeof(int)*sizeq);
            int* stop=(int*)malloc(sizeof(int)*sizeq);

            for(h;h<=sizeq;h++){
                q=str_split(query[h],' ',&sizeq2);
                DR[h]=atoi(q[0]);
                start[h]=atoi(q[1]);
                stop[h]=atoi(q[2]);
            }

            onlineDR=(int*)malloc(sizeof(int)*sizeq);
            for(h=0;h<=sizeq;h++){
                onlineDR[h]=DR[h];
            }

            FILE* f=fopen(name,"r");
            if(f==NULL){
                printf("File doesn't exist\n");
                free(query);
                free(q);
                free(DR);
                free(start);
                free(stop);
                break;
            }
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
            int s=j;
            fclose(f);

            for(i=0;i<=sizeq;i++){
                sendBlock(DR[i], key, name, fileBuf, start[i], stop[i]);
            }

            free(query);
            free(q);
            free(DR);
            free(start);
            free(stop);
            free(fileBuf);
        }

        // GET command
        else if(flag==3){
            size=getSize(name);

            if(size==0){
                printf("Error during reading size of file\n");
                break;
            }

            int sizeq;
            char** query=str_split(buf,' ',&sizeq);
            int i=0;

            char* bufFin=(char*)malloc(sizeof(char)*size);
            for(i;i<size;i++){
				bufFin[i]=0;
			}

            for(i=0;i<sizeq;i++){
                strcat(bufFin,getBlock(atoi(query[i]), key, name, size, sizeq, i));
            }

            FILE* f2=fopen(name,"w");
            if(f2==NULL){
                printf("Erron during opening the file\n");
                free(query);
                free(bufFin);
                break;
            }
            fputs(bufFin,f2);
            fclose(f2);

            free(query);
            free(bufFin);
        }

        else if(flag==4){
            int sizeq;
            char** query=str_split(buf,' ',&sizeq);
            key=query[1];
        }
    }

    // close the socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket");

    if (DEBUG) fprintf(stderr, "Exiting...\n");

    free(key);
    free(onlineDR);
    free(name);
    exit(EXIT_SUCCESS);
}
