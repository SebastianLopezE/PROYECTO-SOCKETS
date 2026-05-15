#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

void MoverArchivo(SOCKET clienteSocket, const char* nombreArchivo){
    FILE* archivo = fopen(nombreArchivo, "rb");
    long long tamArchivo = 0;

    if(archivo == NULL){
        // Si no existe, enviamos tamaño 0 para avisar que falló
        printf("Archivo %s no encontrado, enviando 0 bytes.\n", nombreArchivo);
        send(clienteSocket, (char*)&tamArchivo, sizeof(tamArchivo), 0);
        return;
    }

    fseek(archivo, 0, SEEK_END); 
    tamArchivo = ftell(archivo); 
    fseek(archivo, 0, SEEK_SET);

    send(clienteSocket, (char*)&tamArchivo, sizeof(tamArchivo), 0);

    char bufferArchivo[1024];
    size_t bytesLeidos;
    while((bytesLeidos = fread(bufferArchivo, 1, sizeof(bufferArchivo), archivo)) > 0){ 
        send(clienteSocket, bufferArchivo, bytesLeidos, 0);
    }
    printf("Archivo %s enviado al central\n", nombreArchivo);
    fclose(archivo);
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

// buscar la dirección del servidor central con UDP
struct sockaddr_in encontrarCentral(){
    SOCKET UDPsocket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in escucharAddr, centralAddr;
    int tamAddr = sizeof(centralAddr);
    char buffer[1024];

    escucharAddr.sin_family = AF_INET;
    escucharAddr.sin_port = htons(3491);
    escucharAddr.sin_addr.s_addr = INADDR_ANY;

    bind(UDPsocket, (struct sockaddr*)&escucharAddr, sizeof(escucharAddr));
    printf("buscando servidor central\n");

    int bytesRecv = recvfrom(UDPsocket, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&centralAddr, &tamAddr);
    if (bytesRecv > 0) {
        buffer[bytesRecv] = '\0';
        if(strcmp(buffer, "soy_central") == 0){
            printf("central encontrado ");
        }
    }
    
    closesocket(UDPsocket);
    centralAddr.sin_port = htons(3490); // cambiar al puerto TCP del central
    return centralAddr;
}

DWORD WINAPI funcionHilo(void* data){  
    SOCKET clienteSocket = *(SOCKET*)data; 
    free(data);

    char bufferComando[1024];
    int bytesRecibidos;
    while((bytesRecibidos = recv(clienteSocket, bufferComando, sizeof(bufferComando)- 1, 0)) > 0){
        bufferComando[bytesRecibidos] = '\0'; 
        bufferComando[strcspn(bufferComando, "\r\n")] = 0; 

        if(strcmp(bufferComando, "ls") == 0){
            printf("Central pidio lista de archivos\n");
            MostrarArchivos(clienteSocket);
            continue;
        }else if(strncmp(bufferComando, "up ", 3) == 0){
            char* nombreArchivo = bufferComando + 3;
            recibirArchivo(clienteSocket, nombreArchivo);
            continue;
        }else if(strncmp(bufferComando, "mv ", 3)== 0){
            char* nombreArchivo = bufferComando + 3;
            MoverArchivo(clienteSocket, nombreArchivo);
            continue;
        }else if(strncmp(bufferComando, "echo ", 5) == 0){
            char* mensajeEcho = bufferComando + 5;
            send(clienteSocket, mensajeEcho, strlen(mensajeEcho), 0);
            continue;
        }else if(strcmp(bufferComando, "bye") == 0){
            break;
        }
    }
    closesocket(clienteSocket);
    return 0;
}

int main() {
    WSADATA datosWin;
    SOCKET conexionAlCentral;
    struct sockaddr_in direccion_central;

    if(WSAStartup(MAKEWORD(2,2), &datosWin) != 0) return 1;
    direccion_central = encontrarCentral(); // buscar la dirección del servidor central
    conexionAlCentral = socket(AF_INET, SOCK_STREAM, 0); // crear socket TCP

    if(connect(conexionAlCentral, (struct sockaddr*)&direccion_central, sizeof(direccion_central)) != SOCKET_ERROR) {
        char id[] = "soy_sec";
        send(conexionAlCentral, id, strlen(id), 0);
        printf("conectado al servidor central\n");

        // crear hilo para manejar la comunicación con el central
        SOCKET* pCentral = (SOCKET*)malloc(sizeof(SOCKET));
        *pCentral = conexionAlCentral;

        HANDLE hiloC = CreateThread(NULL, 0, funcionHilo, pCentral, 0, NULL);
        if(hiloC != NULL) {
            WaitForSingleObject(hiloC, INFINITE); 
            CloseHandle(hiloC);
        }
    } else {
        printf("no se pudo conectar al servidor central.\n");
    }

    closesocket(conexionAlCentral);
    WSACleanup();
    return 0;
}