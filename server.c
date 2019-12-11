#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>

#define MAX_BUFFER 500

typedef struct {
    char *buffer[MAX_BUFFER];
    int start, end;
    int top, vain;
    pthread_mutex_t *mutex;
    pthread_cond_t *nfull, *nempty;
} msgqueue;

typedef struct {
    fd_set serverfd;
    int socketf;  
    int clientSocket[MAX_BUFFER];
    char username[MAX_BUFFER][MAX_BUFFER];
    char sbfilename[MAX_BUFFER][MAX_BUFFER];
    int clientnum;
    pthread_mutex_t *mutex;
    msgqueue *queue;
} chat;

typedef struct {
    int clientSocketf;
    char userName[MAX_BUFFER];
    int list;
    chat *home;
} Client;


void *newClientHandler(chat *home);
void *clientHandler(Client *client);
void *messageHandler(chat *home);


int main(int argc, char *argv[])
{
    struct sockaddr_in servaddr;
    int listenfd;

    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
    {
        printf("Error! Socket create failed.\n");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8080);

    if( bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
    {
        printf("Error! Socket bind failed.\n");
        exit(1);
    }

    if( listen(listenfd, 10) == -1 )
    {
        printf("Error! listen failed.\n");
        exit(1);
    }


    msgqueue *q = (msgqueue *)malloc(sizeof(msgqueue));
    if(q == NULL)
    {
        printf("Error! malloc failed.\n");
        exit(1);
    }

    q->vain = 1;
    q->top = q->start = q->end = 0;
    
    if( (q->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL)
    {
        printf("Error! malloc failed.\n");
        exit(1);
    }
    pthread_mutex_init(q->mutex, NULL);

    if( (q->nfull = (pthread_cond_t *) malloc(sizeof(pthread_cond_t))) == NULL)
    {
        printf("Error! malloc failed.\n");
        exit(1);  
    }
    pthread_cond_init(q->nfull, NULL);

    if( (q->nempty = (pthread_cond_t *) malloc(sizeof(pthread_cond_t))) == NULL)
    {
        printf("Error! malloc failed.\n");
        exit(1);
    }
    pthread_cond_init(q->nempty, NULL);


    chat home;

    FD_ZERO(&(home.serverfd));
    FD_SET(listenfd, &(home.serverfd));

    home.socketf = listenfd;

    memset(home.clientSocket, 0, MAX_BUFFER*sizeof(int));
    home.clientnum = 0;
    for(int i=0; i<MAX_BUFFER; i++)
    {
        strcpy(home.username[i], "\0");
        strcpy(home.sbfilename[i], "\0");
    }

    home.queue = q;

    if( (home.mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t))) == NULL)
    {
        printf("Error! malloc failed.\n");
        exit(1);
    }
    pthread_mutex_init(home.mutex, NULL);
    
    pthread_t connectionThread;
    if( (pthread_create(&connectionThread, NULL, (void *)&newClientHandler, &home)) == 0 )
    {
        printf("Start connection handler\n");
    }

    pthread_t messagesThread;
    if((pthread_create(&messagesThread, NULL, (void *)&messageHandler, &home)) == 0)
    {
        printf("Start message handler\n\n");
    }

    pthread_join(connectionThread, NULL);
    pthread_join(messagesThread, NULL);


    pthread_mutex_destroy( (home.queue)->mutex );
    pthread_cond_destroy( (home.queue)->nfull );
    pthread_cond_destroy( (home.queue)->nempty );
    free( (home.queue)->mutex );
    free( (home.queue)->nfull );
    free( (home.queue)->nempty );
    free(home.queue);


    pthread_mutex_destroy(home.mutex);
    free(home.mutex);
    
    close(listenfd);
}


void *newClientHandler(chat *home)
{
    while(1)
    {
        int clientSocketf = accept(home->socketf, NULL, NULL);
        if(clientSocketf > 0)
        {
            printf("New client join! Socket: %d\n", clientSocketf);

            pthread_mutex_lock(home->mutex);
            if(home->clientnum < MAX_BUFFER)
            {   
                Client *cli = (Client*)malloc(sizeof(Client));
                cli->clientSocketf = clientSocketf;
                cli->home = home;

                for(int i = 0; i < MAX_BUFFER; i++)
                {
                    if(!FD_ISSET(home->clientSocket[i], &(home->serverfd)))
                    {
                        char tmp2[MAX_BUFFER];
                        read(clientSocketf, tmp2, MAX_BUFFER-1);

                        home->clientSocket[i] = clientSocketf;
                        strcpy(home->username[i], tmp2);
                        cli->list = i;
                        strcpy(cli->userName, tmp2);
                        
                        break;
                    }
                }
                FD_SET(clientSocketf, &(home->serverfd));

                pthread_t clientThread;
                if( (pthread_create(&clientThread, NULL, (void *)&clientHandler, cli)) == 0 )
                {
                    home->clientnum++;
                    printf("Client join. Socket: %d\n\n", clientSocketf);
                }
                else
                {
                    printf("Error! Client failed to join.\n\n");
                    free(cli);
                    close(clientSocketf);
                }
            }
            pthread_mutex_unlock(home->mutex);
        }
    }
}


void *clientHandler(Client *client)
{
    int clientSocketf = client->clientSocketf;
    chat *home = client->home;
    msgqueue *q = home->queue;

    char msg[MAX_BUFFER], send[MAX_BUFFER];
    char *ptr, tmp[MAX_BUFFER];

    while(1)
    {
        read(clientSocketf, msg, MAX_BUFFER);
        strcpy(tmp, msg);
        ptr = strtok(tmp, " ");

        if(strcmp(msg, "//exit") == 0)
        {   
            pthread_mutex_lock(home->mutex);

            home->clientSocket[client->list] = 0;
            close(clientSocketf);
            home->clientnum--;

            pthread_mutex_unlock(home->mutex);

            strcpy(send, client->userName);
            strcat(send, " offline");
            for(int i=0; i<MAX_BUFFER; i++)
                if(home->clientSocket[i])
                    write(home->clientSocket[i], send, strlen(send) + 1);

            free(client);
            printf("Client on socket %d has disconnected.\n\n", clientSocketf);
            return NULL;
        }
        else if(strcmp(msg, "//show") == 0)
        {
            strcpy(send, "\0");

            for(int i=0; i<MAX_BUFFER; i++)
                if(home->clientSocket[i])
                {
                    strcat(send, home->username[i]);
                    strcat(send, " ");
                }
            write(clientSocketf, send, strlen(send) + 1);
        }
        else if(strcmp(ptr, "//sendto") == 0)
        {
            ptr = strtok(NULL, " ");
            for(int i=0; i<MAX_BUFFER; i++)
            {
                if(strcmp(ptr, home->username[i]) == 0)
                {
                    if(i != client->list)
                    {
                        sprintf(send, "From %s: ", client->userName);

                        while(*ptr!='\0') ptr++;
                        ptr++;
                        strcat(send, ptr);

                        if(write(home->clientSocket[i], send, strlen(send) + 1) == -1)
                            printf("Error! Failed to send message.");
                    }

                    break;
                }
            }
        }
        else if(strcmp(ptr, "//sendfile") == 0)
        {
            ptr = strtok(NULL, " ");
            int i;
            for(i=0; i<MAX_BUFFER; i++)
            {
                if(strcmp(ptr, home->username[i]) == 0)
                {
                    if(i != client->list)
                    {
                        strcpy(send, "Valid.");
                        write(clientSocketf, send, strlen(send)+1);

                        struct sockaddr_in fileaddr;
                        int listenfile;

                        listenfile = socket(AF_INET, SOCK_STREAM, 0);
                        bzero(&fileaddr, sizeof(fileaddr));

                        fileaddr.sin_family = AF_INET;
                        fileaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                        fileaddr.sin_port = htons(8888);

                        bind(listenfile, (struct sockaddr *) &fileaddr, sizeof(fileaddr));
                        listen(listenfile, 1);

                        int clientfile = accept(listenfile, NULL, NULL);
                        
                        ptr = strtok(NULL, " ");
                        strcpy(msg, "Server_");
                        strcat(msg, ptr);
                        FILE *ft=fopen(msg, "wb");
                        int nb;

                        while(1){
                            if( (nb = read(clientfile, tmp, MAX_BUFFER)) == 0)
                                break;

                            fwrite(tmp, sizeof(char), nb, ft);
                        }

                        fclose(ft);
                        close(clientfile);
                        close(listenfile);
                        strcpy(home->sbfilename[i], msg);

                        
                        sprintf(send, "File from %s, do you accept? (/Y or /N)", client->userName);
                        write(home->clientSocket[i], send, strlen(send)+1);
                    }
                    else{
                        strcpy(send, "Invalid.");
                        write(clientSocketf, send, strlen(send)+1);
                    }
                    
                    break;
                }
            }

            if(i == MAX_BUFFER) {
                strcpy(send, "Invalid.");
                write(clientSocketf, send, strlen(send)+1);
                continue;
            }
        }
        else if( (strcmp(msg, "/Y") == 0) || (strcmp(msg, "/N") == 0))
        {
            char fname[MAX_BUFFER];
            strcpy(fname, home->sbfilename[client->list]);

            if(msg[1] == 'Y')
            {
                struct sockaddr_in fileaddr;
                int listenfile;

                listenfile = socket(AF_INET, SOCK_STREAM, 0);
                bzero(&fileaddr, sizeof(fileaddr));

                fileaddr.sin_family = AF_INET;
                fileaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                fileaddr.sin_port = htons(8800);

                bind(listenfile, (struct sockaddr *) &fileaddr, sizeof(fileaddr));
                listen(listenfile, 1);

                write(clientSocketf, fname, strlen(fname)+1);

                int clientfile = accept(listenfile, NULL, NULL);

                FILE *ft = fopen(fname, "rb");
                int nb;
                while(!feof(ft)){
                    nb = fread(msg, sizeof(char), MAX_BUFFER, ft);
                    write(clientfile, msg, nb);
                }

                fclose(ft);
                close(clientfile);
                close(listenfile);
            }

            remove(fname);
            strcpy(home->sbfilename[client->list], "\0");
        }
        else
        {
            while(q->top)
                pthread_cond_wait(q->nfull, q->mutex);
            
            strcpy(send, client->userName);
            strcat(send, ": ");
            strcat(send, msg);

            ptr = (char*)malloc(strlen(send)*sizeof(char) + sizeof(char));
            strcpy(ptr, send);

            pthread_mutex_lock(q->mutex);
            q->buffer[q->end] = ptr;
            q->end++;
            if(q->end == MAX_BUFFER)
                q->end = 0;
            if(q->end == q->start)
                q->top = 1;
            q->vain = 0;

            pthread_mutex_unlock(q->mutex);
            pthread_cond_signal(q->nempty);
        }
    }
}


void *messageHandler(chat *home)
{
    msgqueue *q = home->queue;
    int *clientSocket = home->clientSocket;

    while(1)
    {
        pthread_mutex_lock(q->mutex);
        
        while(q->vain)
            pthread_cond_wait(q->nempty, q->mutex);
        
        char* message = q->buffer[q->start];
        q->start++;
        if(q->start == MAX_BUFFER)
            q->start = 0;
        if(q->start == q->end)
            q->vain = 1;
        q->top = 0;

        pthread_mutex_unlock(q->mutex);
        pthread_cond_signal(q->nfull);
        printf("Broadcast message: \"%s\"\n\n", message);
        for(int i = 0; i < MAX_BUFFER; i++)
        {
            int socket = clientSocket[i];

            if(socket != 0)
                if( (write(socket, message, strlen(message) + 1) == -1) )
                    printf("Erroe! Socket write failed.\n");
        }
        free(message);
    }
}
