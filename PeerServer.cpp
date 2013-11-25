#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/filestream.h"
using namespace std;
using namespace rapidjson;
 
#define SERVER_PORT 5555
#define SIZE_BUFFER 2048
#define PASSWORD "REDE_PDMJ"
 
typedef struct {
    int port;
    struct sockaddr_in addr;
    bool status;
} TClient;
 
const char agentListBack[] = "{\"protocol\":\"pdmj\",\"command\":\"agent-list-back\", \"back\":\"%s\", \"sender\":\"%s\",\"recepient\":\"%s\"}"; 
const char pong[] = "{\"protocol\":\"pdmj\",\"command\":\"pong\", \"sender\":\"%s\",\"recepient\":\"%s\"}"; 
const char authenticateBack[] = "{\"protocol\":\"pdmj\",\"command\":\"authenticate-back\", \"back\":\"%s\", \"sender\":\"%s\",\"recepient\":\"%s\"}"; 
const char archiveListBack[] = "{\"protocol\":\"pdmj\",\"command\":\"archive-list-back\", \"back\":\"%s\", \"sender\":\"%s\",\"recepient\":\"%s\"}"; 
const char archiveRequestBack[] = "{\"protocol\":\"pdmj\",\"command\":\"archive-request-back\", \"back\":\"%s\", \"id\":\"%s\", \"http_endress\":\"%s\", \"md5\":\"%s\", \"sender\":\"%s\",\"recepient\":\"%s\"}"; 
const char f[] = "{\"protocol\":\"pdmj\",\"command\":\"agent-list-back\", \"back\":\"%s\", \"sender\":\"%s\",\"recepient\":\"%s\"}"; 



/* {
 *  file:{id:”1”, nome:”file.txt”, size:”200mb”}, 
 * 	folder:{name:”pasta”, file:{id:”2”, nome:”file1.txt”, size:”100kb”}}
 * }*/

void *threadClient(void * arg)
{
    TClient client = *(TClient*) arg;
    int size;
    //bool loop = true;
    char buffer[SIZE_BUFFER], msg[SIZE_BUFFER];
    char *ip = strdup(inet_ntoa(client.addr.sin_addr));
    int port = ntohs(client.addr.sin_port);
    
    cout << "[SERVIDOR] > Thread Client :" << client.port << " ";
    cout << ip << ":" << port << "\n";
    
    while (1) {    
        size = recv(client.port, buffer, sizeof(buffer), 0);
        if((size == -1) || (size == 0)) { // client disconect
           break;
        } else {
            buffer[size] = '\0';
            //cout << "[SERVIDOR] > Recebeu de " << inet_ntoa(client.addr.sin_addr) << ":" << ntohs(client.addr.sin_port) << ": ";
            //cout << buffer << "\n";
             
            Document doc;
             
            // Verifica se a sintaxe esta correta.
            if (!doc.Parse<0>(buffer).HasParseError()) {
             
                // Acesso aos membros do objeto
                assert(doc.IsObject());    
                 
                assert(doc.HasMember("protocol"));
                assert(doc["protocol"].IsString());
                if(!strcmp("pdmj", doc["protocol"].GetString())) { // mensagem do protocolo pdmj
                     
                    assert(doc.HasMember("command"));
                    assert(doc["command"].IsString());

                     //swicth de comandos
                    if(!strcmp("authenticate", doc["command"].GetString()) && !client.status) { //authenticate
                         
                        cout << "[SERVIDOR] > Recebeu comando authenticate\n";
                        
                        assert(doc.HasMember("passport"));
						assert(doc["passport"].IsString());
						client.status = !strcmp(PASSWORD, doc["passport"].GetString()) ? true : false;

                        sprintf(msg, authenticateBack, (client.status ? "true" : "false") , "127.0.0.1", ip);
                        send(client.port, msg, sizeof(msg), 0);
                         
                    } else if(!strcmp("agent-list", doc["command"].GetString()) && client.status) { //agent-list
						
                        cout << "[SERVIDOR] > Recebeu comando agent-list\n";
                        sprintf(msg, agentListBack, "<IP>,<IP>,<IP>", "127.0.0.1", ip);
                        send(client.port, msg, sizeof(msg), 0); 
                     
                    } else if(!strcmp("ping", doc["command"].GetString()) && client.status) { //ping
                         
                        cout << "[SERVIDOR] > Recebeu comando ping\n";
                        sprintf(msg, pong, "127.0.0.1", ip);
                        send(client.port, msg, sizeof(msg), 0); 
                         
                    } else if(!strcmp("archive-list", doc["command"].GetString()) && client.status) { //archive-list
                         
                        cout << "[SERVIDOR] > Recebeu comando archive-list\n";
                        sprintf(msg, archiveListBack, "lista e arquivos", "127.0.0.1", ip);
                        send(client.port, msg, sizeof(msg), 0); 
                         
                    } else if(!strcmp("archive-request", doc["command"].GetString()) && client.status) { //archive-request
                         
                        cout << "[SERVIDOR] > Recebeu comando archive-request\n";
                        
                        
                        assert(doc.HasMember("id"));
						assert(doc["id"].IsString());
						// criar lista de arquivos local, id = indice do vetor
						//doc["id"].GetString()
                        
                        sprintf(msg, archiveRequestBack, "true", "3", "http://ip:port/file.txt", "MD5-HASH", "127.0.0.1", ip);
                        send(client.port, msg, sizeof(msg), 0); 
                         
                    } else if(!strcmp("end-conection", doc["command"].GetString()) && client.status) { //end-conection
                         
                        cout << "[SERVIDOR] > Recebeu comando end-conection\n";
                        
                        //remove o usuario da lista e termina conexão
                        //loop = false;
                        break;
                         
                    } else {
                        // comando não reconhecido
                        // ou cliente não autentificado
                        cout << "[SERVIDOR] > Comando: Inválido ou não autentificado\n";
                    }
                    
                }
 
            } else {
                cout << "[SERVIDOR] > Recebeu um comando invállido !(JSON)\n";
            }  
        }      
         
    }
    
    cout << "[SERVIDOR] > Cliente Desconectado :" << inet_ntoa(client.addr.sin_addr) << ":" << ntohs(client.addr.sin_port) << "\n";   
    
    close(client.port);
    free(ip);
    
    return NULL;
}
 
int main(int argc, char** argv)
{
    cout << "*** SERVIDOR ***\n\n";
     
    int port;
    struct sockaddr_in local;
     
    /* cria socket. PF_INET define IPv4, SOCK_STREAM define TCP */
    if ((port = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("erro: porta nao foi criada corretamente");
        exit(1);   
    } 
       
    /* porta criada, agora faz o bind com o numero da porta desejado */
    local.sin_family = AF_INET;
    local.sin_port = htons(SERVER_PORT);
    local.sin_addr.s_addr = INADDR_ANY;
    memset(&(local.sin_zero), '\0', 8);
                 
    if (bind(port, (struct sockaddr *) &local, sizeof(struct sockaddr_in)) == -1) {
        perror("erro: nao conseguiu fazer bind\n");
        exit(1);
    }
       
    /* agora faz uma chamada ao listen*/
    if (listen(port, 50) ==-1) {
        perror("erro: problemas com o listen\n");
        exit(1);
    }
     
    TClient client;
    int zsize = sizeof(struct sockaddr_in);
    while (1) {
         
        client.port = accept(port, (struct sockaddr*) &client.addr, (socklen_t *) &zsize);
        if (client.port == -1) {
            perror("erro: accept retornou erro\n");
            exit(1);   
        }  
        
        client.status = false;
         
        cout << "[SERVIDOR] > Cliente Conectado: " << inet_ntoa(client.addr.sin_addr) << ":" << ntohs(client.addr.sin_port) << "\n";  
         
        pthread_t thread;
        pthread_create(&thread, NULL, threadClient, (void*) &client);
         
    }
     
    close(port);
     
    return 0;
}
