#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>

#define MAX_BUFFER 500

void interrupt(int s);

static int socketf;

int main(int argc, char *argv[])
{
    struct sockaddr_in serveraddr;
    struct hostent *Host;

    if(argc != 3)
    {
        printf("please insert your name as following : './client' '[username]' 'localhost'\n");
        exit(1);
    }
    if( (Host = gethostbyname(argv[2])) == NULL )
    {
        printf("Error! Can't get host name.\n");
        exit(1);
    }


    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr = *((struct in_addr *)Host->h_addr_list[0]);
    serveraddr.sin_port = htons(8080);

    if( (socketf = socket(AF_INET, SOCK_STREAM, 0))== -1 )
    {
        printf("Error! Can't create socket.\n");
        exit(1);
    }

    if( connect(socketf, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr)) < 0 )
    {
        printf("Error! Can't connect to server.\n");
        exit(1);
    }


    signal(SIGINT, interrupt);
    
    write(socketf, argv[1], strlen(argv[1]) + 1);

    fd_set clientf;
    char send[MAX_BUFFER], recv[MAX_BUFFER];
    char *ptr, tmp[MAX_BUFFER];
    while(1)
    {
        FD_ZERO(&clientf);
        FD_SET(socketf, &clientf);
        FD_SET(0, &clientf);

        if(select(FD_SETSIZE, &clientf, NULL, NULL, NULL) != -1) 
        {
            if(FD_ISSET(socketf, &clientf))
            {
                read(socketf, recv, MAX_BUFFER - 1);
                    printf("%s\n", recv);
            }
            
            if(FD_ISSET(0, &clientf))
            {
                fgets(send, MAX_BUFFER - 1, stdin);
                send[ strlen(send)-1 ] = '\0';
                
                strcpy(tmp, send);
                ptr = strtok(tmp, " ");
                
                if(strcmp(send, "//exit") == 0)
                {
                    interrupt(-1);
                    exit(1);
                }
                else if(strcmp(ptr, "//sendfile") == 0)
                {
                    if( (ptr = strtok(NULL, " ")) == NULL){  
                        printf("please insert your file as following : '//sendfile' '[user name]' '[file name]'\n");
                        continue;
                    }
                    if( (ptr = strtok(NULL, " ")) == NULL){  
                        printf("please insert your file as following : '//sendfile' '[user name]' '[file name]'\n");
                        continue;
                    }
                    
                    FILE *f = fopen(ptr, "rb");
                    if(f == NULL){
                        printf("Error! File doesn't exist.\n");
                        continue;
                    }

                    if(write(socketf, send, strlen(send) + 1) == -1)
                        printf("Error! Failed sending message.");
                    
                    read(socketf, recv, MAX_BUFFER-1);

                    if(strcmp(recv, "Valid.") == 0)
                    {
                        struct sockaddr_in FileAddr;
                        bzero(&FileAddr, sizeof(FileAddr));
                        FileAddr.sin_family = AF_INET;
                        FileAddr.sin_addr = *((struct in_addr *)Host->h_addr_list[0]);
                        FileAddr.sin_port = htons(8888);

                        int socketfile = socket(AF_INET, SOCK_STREAM, 0);
                        connect(socketfile, (struct sockaddr *) &FileAddr, sizeof(struct sockaddr));

                        int nb;
                        while(!feof(f))
                        {
                            nb = fread(tmp, sizeof(char), MAX_BUFFER, f);
                            write(socketfile, tmp, nb);
                        }

                        fclose(f);
                        close(socketfile);
                        printf("Upload success.\n");
                    }
                    else{
                        printf("Wrong user.\n");
                        continue;
                    }
                }
                else if(strcmp(send, "/Y") == 0)
                {
                    if(write(socketf, send, strlen(send) + 1) == -1)
                        printf("Write failed when sending message.");

                    read(socketf, recv, MAX_BUFFER);
                    char fn[MAX_BUFFER];
                    strcpy(fn, "Download_");
                    strcat(fn, recv);

                    struct sockaddr_in FileAddr;
                    bzero(&FileAddr, sizeof(FileAddr));
                    FileAddr.sin_family = AF_INET;
                    FileAddr.sin_addr = *((struct in_addr *)Host->h_addr_list[0]);
                    FileAddr.sin_port = htons(8800);

                    int socketfile = socket(AF_INET, SOCK_STREAM, 0);
                    connect(socketfile, (struct sockaddr *) &FileAddr, sizeof(struct sockaddr));

                    FILE *f = fopen(fn, "wb");
                    int nb;
                    while( nb = read(socketfile, recv, MAX_BUFFER) )
                        fwrite(recv, sizeof(char), nb, f);

                    fclose(f);
                    close(socketfile);
                    printf("Download success.\n");
                }
                else
                {
                    if(write(socketf, send, strlen(send) + 1) == -1)
                        printf("Write failed when sending message.");
                }
            }
        }
        else
        {
            printf("select() failed\n");
            exit(1);
        }
    }
}

void interrupt(int sig_unused)
{
    char msg[] = "//exit";
    if( write(socketf, msg, strlen(msg) + 1) == -1 )
        printf("Write failed when exiting room.\n");

    close(socketf);
    exit(1);
}
