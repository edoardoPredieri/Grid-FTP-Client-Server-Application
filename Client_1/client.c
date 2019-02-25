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

int ID = 001;

char* getBlock(int DR, char* k, char* n){
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
        sprintf(buf,"Get $%s$%s\n",n,k);
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

        char* sret=(char*)malloc(sizeof(char)*sizeof(buf));
        sprintf(sret,"%s",buf);
        return sret;
    }
    return NULL;
}

void sendBlock(int DR, char* b, char* k, char* n){
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
        sprintf(buf,"Put $%s$%s$%s\n",b,n,k);
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

char** divBuff(char* buf, int size,  int n){
    int i=0;
    char** ret=(char**)malloc(sizeof(char*)*n);
    for(i;i<n;i++){
        ret[i]=(char*)malloc(sizeof(char)*(size/n));
    }

    i=1;
    int j=0;
    for(i;i<=n;i++){
        int k=0;
        for(j;j<(size/n)*i;j++){
            ret[i-1][k]=buf[j];
            k++;
        }
    }
    return ret;
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

int getFlag(char* buf, int* s, char** n, int* p){
    int ret=0;
	if(strncmp (buf, "GetDR",5 )==0){
        *p=1;
		ret=1;
	}
	else if(strncmp (buf, "Put",3 )==0){

        if(*p==0){
            printf("Error you must type GetDR before doing Put\n");
            return 0;
        }

        char** query=str_split(buf,' ');
        char* name=query[1];
        int size=atoi(query[2]);

        *n=name;
        *s=size;
		ret=2;
        *p=2;
	}
	else if(strncmp (buf, "Get",3 )==0){
        if(*p!=2){
            printf("Error you must type Put before doing Get\n");
            return 0;
        }
        char** query=str_split(buf,' ');
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
    int p=0;
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


        flag=getFlag(buf, &size, &name, &p);


        // read message from server
        while ( (msg_len = recv(socket_desc, buf, buf_len, 0)) < 0 ) {
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Cannot read from socket");
        }

        printf("Server response: %s\n", buf); // no need to insert '\0'

        if(flag==1){
			int i=0;

            char** query=str_split(buf,' ');
            onlineDR=(int*)malloc(sizeof(int)*(sizeof(query)/4));

            for(i;i<(sizeof(query)/4);i++){
                onlineDR[i]=atoi(query[i]);
            }

		}

        else if(flag==2 && p){
            FILE* f=fopen(name,"r");
            int o=0;
            while(onlineDR[o]!=0){
                o++;
            }

            int n=1,j=0, i=0;
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

            char** bufList=divBuff(fileBuf, s, o);
            for(i=0;i<o;i++){
                sendBlock(onlineDR[i], bufList[i], key, name);
            }
        }

        else if(flag==3 && p==2){
            char** query=str_split(buf,' ');
            int i=0;

            char* bufFin=(char*)malloc(sizeof(char)*sizeof(query)/4);

            int o=0;
            while(onlineDR[o]!=0){
                o++;
            }
            for(i=0;i<o;i++){
                strcat(bufFin,getBlock(onlineDR[i], key, name));
            }

            FILE* f2=fopen(name,"w");       // change name

            fputs(bufFin,f2);

            fclose(f2);
        }


        else if(flag==4){
            char** query=str_split(buf,' ');
            key=query[1];
        }

    }


    // close the socket
    ret = close(socket_desc);
    ERROR_HELPER(ret, "Cannot close socket");

    if (DEBUG) fprintf(stderr, "Exiting...\n");

    exit(EXIT_SUCCESS);
}
