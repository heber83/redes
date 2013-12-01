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
#include <ifaddrs.h>
#include <dirent.h>
#include "libs/rapidjson/document.h"
#include "libs/md5.h"

using namespace std;
using namespace rapidjson;
 
#define SERVER_PORT 9876
#define SIZE_BUFFER 2048
#define PASSWORD "DiJqWHqKtiDgZySAv7ZX"
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

typedef struct {
	char *name;
	int size;
} sFile;

typedef struct {
	sFile *list;
	int count;
} sListFile;
 
sListFile listFiles;
 
 /* Mensagens resposta */
const char agentListBack[] = "{\"protocol\":\"pdmj\",\"command\":\"agent-list-back\", \"status\":\"%d\", \"back\":\"%s\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char pong[] = "{\"protocol\":\"pdmj\",\"command\":\"pong\", \"status\":\"%d\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char authenticateBack[] = "{\"protocol\":\"pdmj\",\"command\":\"authenticate-back\", \"status\":\"%d\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char archiveListBack[] = "{\"protocol\":\"pdmj\",\"command\":\"archive-list-back\", \"status\":\"%d\", \"back\":\"%s\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 
const char archiveRequestBack[] = "{\"protocol\":\"pdmj\",\"command\":\"archive-request-back\", \"status\":\"%d\", \"id\":\"%d\", \"http_endress\":\"%s\", \"size\":\"%d\", \"md5\":\"%s\", \"sender\":\"%s\",\"receptor\":\"%s\"}"; 

	void *threadClient(void*);
	bool myfolder(char*);
	void myip(char*);
	
	bool add_ip(char*);
	bool remove_ip(char*);
	
	void ler_ips();
	void salvar_ips();
	int fileSize(char*, char*);
	void listaArquivos(char*) ;
 
int main(int argc, char** argv)
{
	myip(meu_ip);
	if(myfolder(minha_pasta)) {
		listaArquivos(minha_pasta);
	}
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
						
						char *lips = (char*) malloc((lista_ip.count*16));
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
						
						listaArquivos(minha_pasta);
						
						char *lfiles = (char*) malloc((listFiles.count*255)), temp[255];
						lfiles[0] = '\0';	
						for(int i = 0; i < listFiles.count; i++) {
							sprintf(temp, "file:[id:\"%d\",nome:\"%s\",size:\"%d\"]", i, listFiles.list[i].name, listFiles.list[i].size);
							strcat(lfiles, temp);
							if(i != (listFiles.count-1))
								strcat(lfiles, ",\n");	
						}				
                        
						if(client.status) {
							sprintf(msg, archiveListBack, MSG_OK, lfiles, meu_ip, ip);
						} else {
							sprintf(msg, archiveListBack, MSG_ACESSO_NAO_AUTORIZADO, "", meu_ip, ip);
						}                        
                        
                        send(client.port, msg, sizeof(msg), 0); 
                        
                        cout << "[=] Comando: archive-list\n";                       
                        cout << "\n[>] Resposta: " << ip << ", " << msg << "\n\n";
                         
                    } else if(!strcmp("archive-request", doc["command"].GetString())) { //archive-request

						if(client.status) {
							
							assert(doc.HasMember("id"));
							assert(doc["id"].IsString());
							int id = atoi(doc["id"].GetString());
							if(listFiles.list[id].name) {
								
								char url[512];
								sprintf(url, "http://%s:%d/%s", meu_ip, 80, listFiles.list[id].name);
																
								MD5 md5;
								char temp[255];
								sprintf(temp, "%s%s", minha_pasta, listFiles.list[id].name);

								sprintf(msg, archiveRequestBack, MSG_ENCONTRADO, id, url, listFiles.list[id].size, md5.digestFile(temp), meu_ip, ip);
							} else {
								sprintf(msg, archiveRequestBack, MSG_ARQUIVO_NAO_ENCONTRADO, 0, "", 0, "", meu_ip, ip);
							}
							
						} else {
							sprintf(msg, archiveRequestBack, MSG_ACESSO_NAO_AUTORIZADO, 0, "", 0, "", meu_ip, ip);
						}  
           
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
	ler_ips(); //atualiza ips
	FILE *fp = fopen(LISTA_IPS, "w");
	if(fp) {
		for(int i = 0; i < lista_ip.count; i++) {
			fprintf(fp, "%s\n", lista_ip.ip[i]);
		}
		fclose(fp);
	}
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

int fileSize(char *folder, char*name) 
{
	char b[2048];
	sprintf(b, "%s%s", folder, name);
	struct stat s;
	if(stat(b, &s) == 0) {
		return s.st_size;
	} else {
		return -1;
	}
}

void listaArquivos(char *folder) 
{
    DIR *dir = opendir(folder);
    struct dirent *entrada = 0;
    int  i;
    
    for(i = 0; i < listFiles.count; i++) {
		free(listFiles.list[i].name);
		free(listFiles.list);
	}
    
    i = listFiles.count = 0;
	
	int *pt = NULL;

    while((entrada = readdir(dir)))
        if(entrada->d_type == 0x8) {
			listFiles.list = (sFile*) realloc(pt, (i+1)*sizeof(sFile));
			if(listFiles.list) pt = (int*)listFiles.list;
			listFiles.list[i].name = strdup(entrada->d_name);
			listFiles.list[i].size = fileSize(folder, entrada->d_name);
			//printf("%d %s %d\n", i, listFiles.list[i].name, listFiles.list[i].size);
			i++;
		}
	listFiles.count = i;
    closedir (dir);
}
