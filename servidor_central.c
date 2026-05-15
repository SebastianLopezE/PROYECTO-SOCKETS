#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

SOCKET socketSec = INVALID_SOCKET; // guarda el socket del secundario conectado

void MostrarTree(SOCKET clienteSocket){
    system("tree /F > ArbolDeArchivos.txt");
    FILE* ArbolArchivos = fopen("ArbolDeArchivos.txt", "rb");
    if(ArbolArchivos == NULL){
        char MensajeError[] = "error al abrir el directorio\n";
        long long tam = strlen(MensajeError);
        send(clienteSocket, (char*)&tam, sizeof(tam), 0);
        send(clienteSocket, MensajeError, tam, 0);
        return;
    }
    fseek(ArbolArchivos, 0, SEEK_END);
    long long tam = ftell(ArbolArchivos);
    fseek(ArbolArchivos, 0, SEEK_SET);
    send(clienteSocket, (char*)&tam, sizeof(tam), 0);
    char bufferArchivos[512];
    size_t leidos;
    while((leidos = fread(bufferArchivos, 1, sizeof(bufferArchivos), ArbolArchivos)) > 0){
        send(clienteSocket, bufferArchivos, leidos, 0);
    }
    fclose(ArbolArchivos);
}

void MoverArchivo(SOCKET clienteSocket, const char* nombreArchivo){
    FILE* archivo = fopen(nombreArchivo, "rb");

    if(archivo != NULL){
        printf("enviando archivo local: %s\n", nombreArchivo);
        fseek(archivo, 0, SEEK_END); 
        long long tamArchivo = ftell(archivo); 
        fseek(archivo, 0, SEEK_SET);

        send(clienteSocket, (char*)&tamArchivo, sizeof(tamArchivo), 0);

        char bufferArchivo[1024];
        size_t bytesLeidos;
        while((bytesLeidos = fread(bufferArchivo, 1, sizeof(bufferArchivo), archivo)) > 0){ 
            send(clienteSocket, bufferArchivo, bytesLeidos, 0);
        }
        printf("archivo local enviado\n");
        fclose(archivo);
        return;
    }

    //si no esta en el local buscar en el secundario
    if(socketSec != INVALID_SOCKET){
        printf("no se encontro en el local. buscando en secundario: %s\n", nombreArchivo);
        
        // enviar comando mv al secundario
        char comando[1024];
        sprintf(comando, "mv %s", nombreArchivo);
        send(socketSec, comando, strlen(comando), 0);

        // recibir el tamaño del archivo desde el secundario
        long long tamArchivo = 0;
        int recibidos = recv(socketSec, (char*)&tamArchivo, sizeof(tamArchivo), 0);

        if(recibidos == sizeof(tamArchivo) && tamArchivo > 0){
            printf("Archivo encontrado en secundario (%lld bytes). Retransmitiendo...\n", tamArchivo);
            
            // enviar tamaño al cliente
            send(clienteSocket, (char*)&tamArchivo, sizeof(tamArchivo), 0);

            // transferir el archivo desde el secundario al cliente
            char bufferPuente[1024];
            long long bytesRestantes = tamArchivo;
            
            while(bytesRestantes > 0){
                int aLeer = (bytesRestantes < sizeof(bufferPuente)) ? (int)bytesRestantes : sizeof(bufferPuente);
                int n = recv(socketSec, bufferPuente, aLeer, 0);
                
                if(n <= 0) break;

                send(clienteSocket, bufferPuente, n, 0);
                bytesRestantes -= n;
            }
            printf("transferencia desde secundario completada.\n");
            return;
        }
    }

    // si no encuentra ningun archivo
    printf("no se pudo encontrar el archivo %s en ningun servidor.\n", nombreArchivo);
    long long cero = 0;
    send(clienteSocket, (char*)&cero, sizeof(cero), 0);
}

void MostrarArchivos(SOCKET clienteSocket){
    system("dir > ListaDeArchivos.txt");
    FILE* NombresArchivos = fopen("ListaDeArchivos.txt", "rb");
    if(NombresArchivos == NULL){
        char MensajeError[] = "error al abrir el directorio\n";
        long long tam = strlen(MensajeError);
        send(clienteSocket, (char*)&tam, sizeof(tam), 0);
        send(clienteSocket, MensajeError, tam, 0);
        return;
    }
    fseek(NombresArchivos, 0, SEEK_END);
    long long tam = ftell(NombresArchivos);
    fseek(NombresArchivos, 0, SEEK_SET);
    send(clienteSocket, (char*)&tam, sizeof(tam), 0);
    char bufferArchivos[512];
    size_t leidos;
    while((leidos = fread(bufferArchivos, 1, sizeof(bufferArchivos), NombresArchivos)) > 0){
        send(clienteSocket, bufferArchivos, leidos, 0);
    }
    fclose(NombresArchivos);
    remove("ListaDeArchivos.txt");
}

void recibirArchivo(SOCKET clienteSocket, const char* nombreArchivo){
    FILE* archivo = fopen(nombreArchivo, "wb"); 
    if(archivo == NULL){
        printf("no se pudo crear el archivo %s\n", nombreArchivo);
        return;
    }
    long long tamArchivo = 0;
    int bytesRecibidos = recv(clienteSocket, (char*)&tamArchivo, sizeof(tamArchivo), 0);
    
    if(bytesRecibidos != sizeof(tamArchivo)){
        printf("no se pudo recibir el archivo\n");
        fclose(archivo);
        return;
    }
    char buffer[1024];
    long long bytesFaltan = tamArchivo;
    
    while(bytesFaltan > 0){ 
        int bytesALeer = (bytesFaltan < sizeof(buffer)) ? (int)bytesFaltan : sizeof(buffer);
        bytesRecibidos = recv(clienteSocket, buffer, bytesALeer, 0);
        
        if(bytesRecibidos <= 0) break;
        
        fwrite(buffer, 1, bytesRecibidos, archivo); 
        bytesFaltan -= bytesRecibidos; 
    }

    if(bytesFaltan == 0) printf("archivo %s recibido\n", nombreArchivo);
    fclose(archivo);
}

DWORD WINAPI hiloAnuncio(void* data){
    SOCKET UDPsocket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in UDPaddr;
    char mensaje[] = "soy_central"; // clave que busca el secundario
    BOOL permiso = TRUE;

    setsockopt(UDPsocket, SOL_SOCKET, SO_BROADCAST, (char*)&permiso, sizeof(permiso)); // permitir broadcast
    UDPaddr.sin_family = AF_INET;
    UDPaddr.sin_port = htons(3491);
    UDPaddr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while(1){
        sendto(UDPsocket, mensaje, strlen(mensaje), 0, (struct sockaddr*)&UDPaddr, sizeof(UDPaddr));
        Sleep(5000); 
    }
    return 0;
} 

DWORD WINAPI funcionHilo(void* data){  
    SOCKET clienteSocket = *(SOCKET*)data; 
    free(data);
    char mensajeBienvenida[] = "comandos: ls, up <archivo>, mv <archivo>, echo <mensaje>, bye\n";
    send(clienteSocket, mensajeBienvenida, strlen(mensajeBienvenida), 0);
    char bufferComando[1024];
    int bytesRecibidos;
    
    while((bytesRecibidos = recv(clienteSocket, bufferComando, sizeof(bufferComando)- 1, 0)) > 0){
        bufferComando[bytesRecibidos] = '\0'; 
        bufferComando[strcspn(bufferComando, "\r\n")] = 0; 

        if (strcmp(bufferComando, "soy_sec") == 0) {
            socketSec = clienteSocket;
            printf("servidor secundario conectado\n");
            return 0; 
        }if(strcmp(bufferComando, "ls") == 0){
            printf("el cliente envio: %s\n", bufferComando);
            char* bufferCentral = NULL;
            long long tamCentral = 0;
            system("dir > ListaDeArchivos.txt");
            FILE* f = fopen("ListaDeArchivos.txt", "rb");
            if(f) {
                fseek(f, 0, SEEK_END);
                tamCentral = ftell(f);
                fseek(f, 0, SEEK_SET);
                if(tamCentral > 0) {
                    bufferCentral = (char*)malloc((size_t)tamCentral+1);
                    fread(bufferCentral, 1, (size_t)tamCentral, f);
                    bufferCentral[tamCentral] = '\0';
                }
                fclose(f);
            }
            remove("ListaDeArchivos.txt");
            if(!bufferCentral || tamCentral == 0) {
                char* sinArchivos = (char*)"(sin archivos)\n";
                bufferCentral = sinArchivos;
                tamCentral = strlen(sinArchivos);
            }

            // obtener archivos del secundario
            char* bufferSec = NULL;
            long long tamSec = 0;
            int secOk = 0;
            if(socketSec != INVALID_SOCKET){
                send(socketSec, "ls", 2, 0); // pedir lista al secundario
                int recibidos = recv(socketSec, (char*)&tamSec, sizeof(tamSec), 0);
                if(recibidos == sizeof(tamSec) && tamSec > 0){
                    bufferSec = (char*)malloc((size_t)tamSec+1);
                    long long total = 0;
                    while(total < tamSec){
                        int n = recv(socketSec, bufferSec+total, (int)(tamSec-total), 0);
                        if(n <= 0) break;
                        total += n;
                    }if(total == tamSec){
                        bufferSec[tamSec] = '\0';
                        secOk = 1;
                    }else{
                        if(bufferSec) free(bufferSec);
                        bufferSec = NULL;
                        tamSec = 0;
                    }
                }
            }
            // Unir los mensajes
            char encabezadoCentral[] = "--- ARCHIVOS CENTRAL ---\n";
            char encabezadoSec[] = "\n--- ARCHIVOS SECUNDARIO ---\n";
            long long tamTotal = strlen(encabezadoCentral) + (tamCentral > 0 ? tamCentral : 0);
            if(secOk && bufferSec && tamSec > 0) tamTotal += strlen(encabezadoSec) + tamSec;
            // enviar tamaño total 
            send(clienteSocket, (char*)&tamTotal, sizeof(tamTotal), 0);
            // enviar central
            send(clienteSocket, encabezadoCentral, strlen(encabezadoCentral), 0);
            if(bufferCentral && tamCentral > 0) send(clienteSocket, bufferCentral, (int)tamCentral, 0);
            // enviar secundario si se recibio bien
            if(secOk && bufferSec && tamSec > 0){
                send(clienteSocket, encabezadoSec, strlen(encabezadoSec), 0);
                send(clienteSocket, bufferSec, (int)tamSec, 0);
            }
            if(bufferCentral && bufferCentral != (char*)"(sin archivos)\n") free(bufferCentral);
            if(bufferSec) free(bufferSec);
            continue;
        }else if(strncmp(bufferComando, "up ", 3) == 0){
            printf("el cliente envio: %s\n", bufferComando);
            char* nombreArchivo = bufferComando + 3;
            recibirArchivo(clienteSocket, nombreArchivo);
            continue;
        }else if(strcmp(bufferComando, "bye") == 0){
            printf("el cliente envio: %s\n", bufferComando);
            printf("cliente se desconecto\n");
            break;
        }else if(strncmp(bufferComando, "mv ", 3)== 0){
            printf("el cliente envio: %s\n", bufferComando);
            char* nombreArchivo = bufferComando + 3;
            MoverArchivo(clienteSocket, nombreArchivo);
            continue;
        }else if(strncmp(bufferComando, "echo ", 5) == 0){
            printf("el cliente envio: %s\n", bufferComando);
            char* mensajeEcho = bufferComando + 5;
            send(clienteSocket, mensajeEcho, strlen(mensajeEcho), 0);
            continue;
        }else if(strncmp(bufferComando, "cat ", 4)== 0){
            printf("el cliente envio: %s\n", bufferComando);
            char* nombreArchivo = bufferComando + 4;
            FILE* archivo = fopen(nombreArchivo, "rb");
            if(archivo == NULL){
                char MensajeError[] = "error al abrir el archivo\n";
                long long tam = strlen(MensajeError);
                send(clienteSocket, (char*)&tam, sizeof(tam), 0);
                send(clienteSocket, MensajeError, tam, 0);
                continue;
            }
            fseek(archivo, 0, SEEK_END);
            long long tam = ftell(archivo);
            fseek(archivo, 0, SEEK_SET);
            send(clienteSocket, (char*)&tam, sizeof(tam), 0);
            char bufferArchivo[512];
            size_t leidos;
            while((leidos = fread(bufferArchivo, 1, sizeof(bufferArchivo), archivo)) > 0){
                send(clienteSocket, bufferArchivo, leidos, 0);
            }
            fclose(archivo);
            continue;
        }else if(strcmp(bufferComando, "tree") == 0){
            printf("el cliente envio: %s\n", bufferComando);
            MostrarTree(clienteSocket);
            continue;

        }else{
            char msg_invalido[] = "comando incorrecto\n";
            send(clienteSocket, msg_invalido, strlen(msg_invalido), 0);
            continue;
        }
    }

    if(clienteSocket == socketSec) socketSec = INVALID_SOCKET;
    printf("cliente se desconecto\n");
    closesocket(clienteSocket);
    return 0;
}

int main(){
    WSADATA datosWin;
    SOCKET servidor, cliente;
    struct sockaddr_in direccion_servidor, direccion_cliente;
    int tam_direccion;

    if(WSAStartup(MAKEWORD(2,2), &datosWin) != 0) return 1;

    servidor = socket(AF_INET, SOCK_STREAM, 0); 
    if(servidor == INVALID_SOCKET) return 1;

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(3490);   
    direccion_servidor.sin_addr.s_addr = INADDR_ANY; 

    if(bind(servidor, (struct sockaddr*)&direccion_servidor, sizeof(direccion_servidor)) == SOCKET_ERROR) return 1;

    listen(servidor, 5); 
    printf("servidor central esperando en el puerto 3490\n");

    CreateThread(NULL, 0, hiloAnuncio, NULL, 0, NULL); // Iniciar anuncios UDP

    while(1){   
        tam_direccion = sizeof(direccion_cliente);
        cliente = accept(servidor, (struct sockaddr*)&direccion_cliente, &tam_direccion); 
        if(cliente != INVALID_SOCKET){
            printf("se conecto un cliente desde %s | %d\n", inet_ntoa(direccion_cliente.sin_addr), ntohs(direccion_cliente.sin_port));
            SOCKET* ClienteNuevo = (SOCKET*)malloc(sizeof(SOCKET)); 
            *ClienteNuevo = cliente;
            CloseHandle(CreateThread(NULL, 0, funcionHilo, ClienteNuevo, 0, NULL));
        }
    }

    WSACleanup();
    return 0;
}