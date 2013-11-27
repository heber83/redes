#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rapidjson/document.h"
#include <ifaddrs.h>

using namespace std;
using namespace rapidjson;
 
#define SERVER_PORT 5555
#define SIZE_BUFFER 2048
#define PASSWORD "REDE_PDMJ"
#define LISTA_IPS "files/ip.txt"

#define MSG_CONTINUAR 100				// Quando o ping foi satisfeito.
#define MSG_OK 200						// Comandos quando responderam corretamente.
#define MSG_NAO_AUTORIZADO 203			// Chave errada na autenticação.
#define MSG_ENCONTRADO 302				// Arquivo foi encontrado.
#define MSG_REQUISICAO_INVALIDA 400		// Não está no formato ou informação faltando.
#define MSG_ACESSO_NAO_AUTORIZADO 401	// Solicitação por alguém não autenticada.
#define MSG_ARQUIVO_NAO_ENCONTRADO 404	// Arquivo não disponível.
#define MSG_TIMEOUT 408					// Não respondeu a tempo. Obs.: Uso para log.
#define MSG_NAO_IMPLEMENTADO 501		// Ainda falta implementar.

char minha_pasta[255];
char meu_ip[16];

typedef struct {
    char *ip[16];
    int count;
} TIPList;

TIPList lista_ip;

typedef struct {
    int port;
    struct sockaddr_in addr;
    bool status;
} TClient;
 
 /* Mensagens resposta */
const char agentListBack[] = "{\"protocol\":\"pdmj\",\"command\":\"agent-list-back\", \"status\":\"%d\", \"back\":\"%s\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char pong[] = "{\"protocol\":\"pdmj\",\"command\":\"pong\", \"status\":\"%d\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char authenticateBack[] = "{\"protocol\":\"pdmj\",\"command\":\"authenticate-back\", \"status\":\"%d\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char archiveListBack[] = "{\"protocol\":\"pdmj\",\"command\":\"archive-list-back\", \"status\":\"%d\", \"back\":\"%s\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char archiveRequestBack[] = "{\"protocol\":\"pdmj\",\"command\":\"archive-request-back\", \"status\":\"%d\", \"id\":\"%s\", \"http_endress\":\"%s\", \"size\":\"%s\", \"md5\":\"%s\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 

/* {
 *  file:{id:”1”, nome:”file.txt”, size:”200mb”}, 
 * 	folder:{name:”pasta”, file:{id:”2”, nome:”file1.txt”, size:”100kb”}}
 * }*/

	void *threadClient(void*);
	bool myfolder(char*);
	void myip(char*);
	
	bool add_ip(char*);
	bool remove_ip(char*);
	
	void ler_ips();
	void salvar_ips();
 
int main(int argc, char** argv)
{
	myip(meu_ip);
	myfolder(minha_pasta);
	lista_ip.count = 0;
	ler_ips();						
	
    cout << "*** SERVIDOR PEER : " << meu_ip << " ***\n\n";
     
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
                 
    // bind : associa a porta ao servidor
    if (bind(port, (struct sockaddr *) &local, sizeof(struct sockaddr_in)) == -1) {
        perror("erro: nao conseguiu fazer bind\n");
        exit(1);
    }
       
    /* agora faz uma chamada ao listen*/
    // escuta na porta por 50 conexões
    if (listen(port, 50) ==-1) {
        perror("erro: problemas com o listen\n");
        exit(1);
    }
     
    TClient client;
    int zsize = sizeof(struct sockaddr_in);
    
    // pega as conexões e cria uma thread para cada uma
    while (1) {
         
        client.port = accept(port, (struct sockaddr*) &client.addr, (socklen_t *) &zsize);
        if (client.port == -1) {
            perror("erro: accept retornou erro\n");
            exit(1);   
        }  
        
        client.status = false;
         
        cout << "[+] Conexão: " << inet_ntoa(client.addr.sin_addr) << "\n";  
         
        pthread_t thread;
        pthread_create(&thread, NULL, threadClient, (void*) &client);
              
		salvar_ips(); // salva ips
    }
     
    close(port);
     
    return 0;
}

void *threadClient(void * arg)
{
    TClient client = *(TClient*) arg;
    int size, port = ntohs(client.addr.sin_port);
    char buffer[SIZE_BUFFER], msg[SIZE_BUFFER], *ip = strdup(inet_ntoa(client.addr.sin_addr));

    cout << "[+] Thread: " << ip << "\n";
    
    while (1) {    
        size = recv(client.port, buffer, sizeof(buffer), 0);
        if((size == -1) || (size == 0)) { // client disconect
           break;
        } else {
            buffer[size] = '\0';
            cout << "\n[<] Mensagem: " << ip << ", " << buffer << "\n\n";

            Document doc;
             
            // Verifica se a sintaxe json esta correta.
            if (!doc.Parse<0>(buffer).HasParseError()) {
             
                // Acesso aos membros do objeto json
                assert(doc.IsObject());    
                 
                assert(doc.HasMember("protocol"));
                assert(doc["protocol"].IsString());
                if(!strcmp("pdmj", doc["protocol"].GetString())) { // mensagem do protocolo pdmj
                     
                    assert(doc.HasMember("command"));
                    assert(doc["command"].IsString());

                     // swicth de comandos
                    if(!strcmp("authenticate", doc["command"].GetString())) { //authenticate

                        assert(doc.HasMember("passport"));
						assert(doc["passport"].IsString());
						
						if(strcmp(PASSWORD, doc["passport"].GetString()) == 0) {
							client.status = true;
							add_ip(ip);
							sprintf(msg, authenticateBack, MSG_OK, meu_ip, ip);
						} else {
							sprintf(msg, authenticateBack, MSG_NAO_AUTORIZADO, meu_ip, ip);
						}
                        
                        send(client.port, msg, sizeof(msg), 0);
 
                        cout << "[=] Comando: authenticate\n";
                        cout << "\n[>] Resposta: " << ip << ", " << msg << "\n\n";
                         
                    } else if(!strcmp("agent-list", doc["command"].GetString())) { //agent-list
						
						char *lips = (char*) malloc((lista_ip.count*16)+1);
						lips[0] = '\0';	
						for(int i = 0; i < lista_ip.count; i++) {
							strcat(lips, lista_ip.ip[i]);
							if(i != (lista_ip.count-1))
								strcat(lips, ",");
						}	
						if(client.status) {
							sprintf(msg, agentListBack, MSG_OK, lips, meu_ip, ip);
						} else {
							sprintf(msg, agentListBack, MSG_ACESSO_NAO_AUTORIZADO, "", meu_ip, ip);
						}
						
                        send(client.port, msg, sizeof(msg), 0); 
                        
                        cout << "[=] Comando: agent-list\n";
                        cout << "\n[>] Resposta: " << ip << ", " << msg << "\n\n";
                     
                    } else if(!strcmp("ping", doc["command"].GetString())) { //ping
                         
                        sprintf(msg, pong, MSG_CONTINUAR, meu_ip, ip);
                        send(client.port, msg, sizeof(msg), 0); 
                        
                        cout << "[=] Comando: ping\n";
                        cout << "\n[>] Resposta: " << ip << ", " << msg << "\n\n";
                         
                    } else if(!strcmp("archive-list", doc["command"].GetString())) { //archive-list
                         
                        /* falta: criar lista de arquivos e pastas
                         * lista com id, nome, tamanho, md5
                         * */
                         
                        sprintf(msg, archiveListBack, MSG_NAO_IMPLEMENTADO, "", meu_ip, ip);
                        send(client.port, msg, sizeof(msg), 0); 
                        
                        cout << "[=] Comando: archive-list\n";                       
                        cout << "\n[>] Resposta: " << ip << ", " << msg << "\n\n";
                         
                    } else if(!strcmp("archive-request", doc["command"].GetString())) { //archive-request

						/* falta: criar lista de arquivos
						 * endereço http do arquivo
						 * */

                        assert(doc.HasMember("id"));
						assert(doc["id"].IsString());
						// criar lista de arquivos local, id = indice do vetor
						//doc["id"].GetString()
                        
                        //sprintf(msg, archiveRequestBack, <codigo>, "3", "http://ip:port/file.txt", "size", "MD5-HASH", meu_ip, ip);
                        sprintf(msg, archiveRequestBack, MSG_NAO_IMPLEMENTADO, "", "", "", "", meu_ip, ip);
                        send(client.port, msg, sizeof(msg), 0); 
                        
                        cout << "[=] Comando: archive-request\n";
                        cout << "\n[>] Resposta: " << ip << ", " << msg << "\n\n";
                         
                    } else if(!strcmp("end-conection", doc["command"].GetString())) { //end-conection
                        
                        remove_ip(ip);
                        salvar_ips(); 
                         
                        cout << "[=] Comando: end-conection\n";

                        break;
                         
                    } else {
                        // comando não reconhecido
                        cout << "[=] Comando: inválido\n";
                    }
                    
                }
                
                ler_ips(); // atualiza ips
 
            } else {
                cout << "[=] Comando: !(JSON)\n";
            }  
        }      
         
    }
    
    cout << "[-] Desconexão: " << ip << "\n";   
    cout << "[-] Thread: " << ip << "\n";
     
    close(client.port);
    free(ip);

    return NULL;
}

void strip(char *s) {
    char *p2 = s;
    while(*s != '\0') {
    	if(*s != '\n' && *s != '\r') {
    		*p2++ = *s++;
    	} else {
    		++s;
    	}
    }
    *p2 = '\0';
}

bool add_ip(char *buffer)
{
	int i = 0;
	for(i = 0; i < lista_ip.count; i++) {
		if(!strcmp(lista_ip.ip[i], buffer))
			return false;
	}
	lista_ip.ip[lista_ip.count] = (char*) malloc((lista_ip.count+1)*16);
	strcpy(lista_ip.ip[lista_ip.count], buffer);
	lista_ip.count++;
	return true;
}

bool remove_ip(char *buffer)
{
	int i = 0;
	for(i = 0; i < lista_ip.count; i++) {
		if(!strcmp(lista_ip.ip[i], buffer)) {
			lista_ip.ip[i] = '\0';
			return true;
		}
	}
	return false;	
}

void ler_ips() 
{
	char buffer[16];
	FILE *fp = fopen(LISTA_IPS, "r");
	if(fp) {
		while(fgets(buffer, 16, fp) != NULL) {
			strip(buffer); // remove \n
			add_ip(buffer);			
		}
		fclose(fp);
	}
}

void salvar_ips() 
{
	// falta
}

bool myfolder(char *buffer)
{
	char login[32];
	getlogin_r(login, 31);
	sprintf(buffer, "/home/%s/pcmj/", login);

	struct stat st;
	return (stat(buffer, &st) == 0);
}

void myip(char *buffer) 
{
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if(!strcmp("wlan0", ifa->ifa_name))
				strcpy(buffer, addressBuffer);
        } 
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);	
}
