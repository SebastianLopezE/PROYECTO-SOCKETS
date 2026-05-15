#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

void recibirArchivo( SOCKET cliente, const char *nombreArchivo){
    FILE* archivo = fopen(nombreArchivo, "wb"); 
    if(archivo == NULL){
        printf("no se pudo crear el archivo %s\n", nombreArchivo);
        return;
    }
    long long tamArchivo = 0;
    int bytesRecibidos = recv(cliente, (char*)&tamArchivo, sizeof(tamArchivo), 0);
    
    if(bytesRecibidos != sizeof(tamArchivo)){
        printf("no se pudo recibir el archivo\n");
        fclose(archivo);
        return;
    }
    char buffer[1024];
    long long bytesFaltan = tamArchivo;
    
    while(bytesFaltan > 0){
        int bytesALeer = (bytesFaltan < sizeof(buffer)) ? (int)bytesFaltan : sizeof(buffer);
        bytesRecibidos= recv(cliente, buffer, bytesALeer, 0);
        if(bytesRecibidos <= 0){
            printf("error al recibir el archivo\n");
            break;
        }
        fwrite(buffer, 1, bytesRecibidos, archivo);
        bytesFaltan -= bytesRecibidos;
    }

    if(bytesFaltan == 0){
        printf("archivo %s recibido\n", nombreArchivo);
    }
    fclose(archivo);
}

int main(){
    WSADATA datosWin;
    SOCKET cliente;
    int leidos;
    char BuferMensaje[1024];
    struct sockaddr_in direccion_servidor;
    char ip_servidor[32];

    printf("IP del servidor: ");
    scanf("%31s", ip_servidor);
    getchar();

    if(WSAStartup(MAKEWORD(2,2), &datosWin) != 0){
        printf("No se pudo iniciar Winsock\n");
        return 1;
    }
    cliente = socket(AF_INET, SOCK_STREAM, 0);

    if(cliente == INVALID_SOCKET){
        printf("No se pudo crear el socket\n");
        WSACleanup();
        return 1;
    }

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(3490);
    direccion_servidor.sin_addr.s_addr = inet_addr(ip_servidor);

    if(connect(cliente, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) == SOCKET_ERROR){
        printf("No se pudo conectar al servidor\n");
        closesocket(cliente);
        WSACleanup();
        return 1;
    }

    printf("conectado al servidor\n");
    leidos = recv(cliente, BuferMensaje, sizeof(BuferMensaje) -1, 0);
    if (leidos > 0){
        BuferMensaje[leidos] = '\0';
        printf("mensaje del servidor: %s", BuferMensaje);
    }

    while(1){
        printf("comando: ");
        fgets(BuferMensaje, sizeof(BuferMensaje), stdin);
        BuferMensaje[strcspn(BuferMensaje, "\n")] = 0;

        if(strcmp(BuferMensaje, "bye") == 0){
            send(cliente, BuferMensaje, strlen(BuferMensaje), 0);   
            break;
        }else if(strncmp(BuferMensaje, "up ", 3) == 0){
            char *nombreArchivo = BuferMensaje + 3;
            FILE *archivo = fopen(nombreArchivo, "rb");
            if(archivo == NULL){
                printf("no se pudo abrir el archivo %s\n", nombreArchivo);
                continue;
            }

            fseek(archivo, 0, SEEK_END);
            long long tamArchivo = ftell(archivo);
            fseek(archivo, 0, SEEK_SET);

            send(cliente, BuferMensaje, strlen(BuferMensaje), 0);
            send(cliente, (char*)&tamArchivo, sizeof(tamArchivo), 0);

            char bufferArchivo[1024];
            size_t bytesLeidos;
            while((bytesLeidos = fread(bufferArchivo, 1, sizeof(bufferArchivo), archivo)) > 0){
                send(cliente, bufferArchivo, bytesLeidos, 0);
            }
            printf("archivo enviado\n");
            fclose(archivo);
            continue; 
        } else if(strncmp(BuferMensaje, "mv ", 3) == 0){
            char *nombreArchivo = BuferMensaje + 3;
            send(cliente, BuferMensaje, strlen(BuferMensaje), 0);
            recibirArchivo(cliente, nombreArchivo);
            continue;            
        }else if(strcmp(BuferMensaje, "ls")==0){
            send(cliente, BuferMensaje, strlen(BuferMensaje), 0);
            long long tam = 0;
            int recibidos = recv(cliente, (char*)&tam, sizeof(tam), 0);
            if(recibidos != sizeof(tam) || tam <= 0){
                printf("Error al recibir la lista de archivos\n");
                continue;
            }
            char* bufferLs = (char*)malloc((size_t)tam+1);
            long long total = 0;
            while(total < tam){
                int n = recv(cliente, bufferLs+total, (int)(tam-total), 0);
                if(n <= 0) break;
                total += n;
            }
            bufferLs[tam] = '\0';
            printf("archivos en el servidor:\n%s\n", bufferLs);
            free(bufferLs);
            continue;
        }else if(strncmp(BuferMensaje, "cat ", 4) == 0){
            send(cliente, BuferMensaje, strlen(BuferMensaje), 0);
            long long tam = 0;
            
            int recibidos = recv(cliente, (char*)&tam, sizeof(tam), 0);
            if(recibidos != sizeof(tam) || tam <= 0){
                printf("Error al recibir el archivo\n");
                continue;
            } 
            char* bufferCat = (char*)malloc((size_t)tam+1);
            long long total = 0;
            while(total < tam){
                int n = recv(cliente, bufferCat+total, (int)(tam-total), 0);
                if(n <= 0) break;
                total += n;
            }
            bufferCat[tam] = '\0';
            printf("contenido del archivo:\n%s\n", bufferCat);
            free(bufferCat);
            continue;

        }else if(strcmp(BuferMensaje, "tree")== 0){
            send(cliente, BuferMensaje, strlen(BuferMensaje), 0);
            long long tam = 0;
            int recibidos = recv(cliente, (char*)&tam, sizeof(tam), 0);
            if(recibidos != sizeof(tam) || tam <= 0){
                printf("Error al recibir la lista de archivos\n");
                continue;
            }
            char* bufferLs = (char*)malloc((size_t)tam+1);
            long long total = 0;
            while(total < tam){
                int n = recv(cliente, bufferLs+total, (int)(tam-total), 0);
                if(n <= 0) break;
                total += n;
            }
            bufferLs[tam] = '\0';
            printf("estructura del servidor:\n%s\n", bufferLs);
            free(bufferLs);
            continue;
        } else if(send(cliente, BuferMensaje, strlen(BuferMensaje), 0) == SOCKET_ERROR){ 
            printf("error al enviar");
        }
        leidos = recv(cliente, BuferMensaje, sizeof(BuferMensaje) -1, 0);
        if(leidos > 0 ){
            BuferMensaje[leidos]= '\0';
            printf("el servidor dice: %s\n", BuferMensaje);
        }else{
            printf("error en la respuesta");
        }
    }
    closesocket(cliente);
    WSACleanup();
    return 0;
}