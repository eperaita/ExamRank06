#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 




//---------------- GLOBAL  DATA ( fd_set ,client arrays,  ints , buffers )  ----------------//

fd_set ready_to_read; 
fd_set ready_to_write; 
fd_set active_sockets; 


//Client arrays (cada cliente se colocara en el numero de su fd, aunque haya huecos vacios. Message es solo para saber si hay una str pendiente y se concatene)


int ids[65536]; 
int message[65536]; 


//Buffers (buffer de lectura, de escritura y  temporal para almacenar segun va leyendo)

char buffer_read[4096 * 42];  
char buffer_write[4096 * 42]; 
char buffer_str[4096 * 42];  

//ints
int nclients = 0; 
int maxFd = 0; 

//---------------- SIDE FUNCTIONS   ----------------//

void broadcast(int me) 
{
    for(int fd = 0; fd <= maxFd; fd++)
    {
        if(FD_ISSET(fd, &ready_to_write) && me != fd)
            send(fd, buffer_write, strlen(buffer_write), 0);
    }
}

void error(char *error){
    write(2, error, strlen(error));
    exit(1);
};


//----------------------------------------//
//---------------- MAIN ------------------//
//----------------------------------------//

int main(int argc, char **argv){

    if (argc != 2)
        error("Wrong number of arguments\n");

//---------------- SERVER ----------------//

    //CONFIG SERVER - struct sockaddr_in
    struct sockaddr_in server_add;
    
    server_add.sin_family = AF_INET;
    server_add.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1 // el numero tocho ese porque y como lo hago sino 
    int port = atoi(argv[1]);
    server_add.sin_port = htons(port); // esto tambien en hexadecimal creo 

    //ASSIGNAR FD AL SERVER - socket()
    int server_sock;

    if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        error("Fatal error\n");

    //BINDEAR SOCKET AL ADDRESS - bind()
    if(bind(server_sock, (struct sockaddr*)&server_add, sizeof(server_add)) == -1)
    {
        close(server_sock); // cerrar el socket !!!!!!
        error("Fatal error\n");
    }
    //A ESCUCHAR - listen() 
    if(listen(server_sock, 10) == -1) //cuantos a la cola????
    {
        close(server_sock); // cerrar el socket !!!!!!
        error("Fatal error\n");
    }

    // FD DATA ACTIVE 
    FD_ZERO(&active_sockets); //inicializa a 0
    FD_SET(server_sock, &active_sockets); // llena el socket del server en el set 

    maxFd = server_sock;

//---------------- RUN ----------------//

    
    //BUCLE DE ESPERA A EVENTOS
    while(1){
        
        //TODOS READY 
        ready_to_read = active_sockets;
        ready_to_write = active_sockets;

        //select() - int select(int nfds, fd_set *readfds,fd_set * writefds,fd_set * exceptfds,struct timeval * timeout)   
        if(select(maxFd + 1, &ready_to_read, &ready_to_write, 0, 0) <= 0)  //checkea  los sets a ver si estan "ready" (han cambiado de estado)
            continue; //si no recibe nada (0) - sigue dando vueltas y escuchando


        //BUCLE POR TODOS LOS CLIENTES MIRANDO LOS CAMBIOS. 
        
        for(int fd = 0; fd <= maxFd; fd++) 
        {

            if(FD_ISSET(fd, &ready_to_read)) 
            {

                // if SERVER ready (cli 0 ) -  ADD CLIENT 
                if(fd == server_sock){             

                    //New data and socket
                    struct sockaddr_in client_add;
                    socklen_t len = sizeof(client_add);
                    int client_sock = accept(fd, (struct sockaddr*)&client_add, &len);
                    if(client_sock == -1)
                        continue;
                    
                    // accept() and add data to client array in [clients_sock]position
                    FD_SET(client_sock, &active_sockets); //id_by_sock[client_sock] = numberofClients++;???????????
                    ids[client_sock] = nclients;
                    nclients++;
                    message[client_sock] = 0;
                    if(maxFd < client_sock) //actualizar valor de maxfd, en algun momento puede ser uno mas pequeñoo ? da igual 
                        maxFd = client_sock;
                    
                    //broadcast
                    sprintf(buffer_write, "server: client %d just arrived\n", ids[client_sock]); 
                    broadcast(client_sock);
                    
                    break;

                }

                // if CLIENTE ready  - cliente se ha ido o  ha escrito mensaje 
                else{
                        //recibimos msg - recv()
                        int msg = recv(fd, buffer_read, 4096 * 42, 0); 

                        if( msg <= 0){ //  - CLIENT LEFT - 
                        {
                            //broadcast
                            sprintf(buffer_write, "server: client %d just left\n", ids[fd]);
                            broadcast(fd);

                            //out of fd_set and close socket (en el array de clients no hace falta limpiar porque no estara en el fd_set -guarrada-)
                            FD_CLR(fd, &active_sockets);
                            close(fd);

                        }
                        }
                        else{ // -MESSAGE FROM CLIENT . el mensaje ya esta en el buffer_to_read desde recv()

                            //copiamos todo el mensaje a buffer_str
                            for (int i= 0, j = 0; i < msg; i++, j++)
                            {
                                buffer_str[j] = buffer_read[i];
                                if (buffer_str[j] == '\n') //fin de linea
                                {   
                                    buffer_str[j+1] = '\0';
                                    if(message[fd])
                                        sprintf(buffer_write, "%s", buffer_str);
                                    else
                                        sprintf(buffer_write, "client %d: %s", ids[fd], buffer_str);

                                    message[fd] = 0;
                                    broadcast(fd);
                                    j = -1;
                                }
                                else if(i == (msg - 1)) //fin de mensaje sin \n, concatenar siguiente str, osea message=1
                                { 
                                    buffer_str[j+1] = '\0';
                                    if(message[fd])
                                        sprintf(buffer_write, "%s", buffer_str);
                                    else
                                        sprintf(buffer_write, "client %d: %s", ids[fd], buffer_str);
                                        
                                    message[fd] = 1;
                                    broadcast(fd);
                                }
                            }
                        }
                    break;
                }
            }
        }
        

    };

return 0;
};