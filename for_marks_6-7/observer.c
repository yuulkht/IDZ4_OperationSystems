#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define BUFSIZE 256
#define TERMINATE_MSG "SERVER_TERMINATE"  // Сообщение для завершения работы сервера

void DieWithError(char *errorMessage);
void HandleSigint(int sig);  // Объявление обработчика сигнала

int sock;  // Глобальная переменная для сокета

int main(int argc, char *argv[]) {
    struct sockaddr_in servAddr;  // Структура для хранения адреса сервера
    unsigned short servPort;  // Порт сервера
    char *servIP;  // IP-адрес сервера
    char buffer[BUFSIZE];  // Буфер для приема сообщений
    int recvMsgSize;  // Размер полученного сообщения

    // Проверка корректности количества аргументов командной строки
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <IP сервера> <Порт сервера>\n", argv[0]);
        exit(1);
    }

    servIP = argv[1];  // IP-адрес сервера из аргументов командной строки
    servPort = atoi(argv[2]);  // Порт сервера из аргументов командной строки

    // Создание сокета
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() не удалось создать");

    // Заполнение структуры адреса сервера
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(servIP);
    servAddr.sin_port = htons(servPort);

    char observerMessage[] = "Наблюдатель подключен";
    // Отправка сообщения о подключении наблюдателя серверу
    if (sendto(sock, observerMessage, strlen(observerMessage), 0,
               (struct sockaddr *) &servAddr, sizeof(servAddr)) != strlen(observerMessage))
        DieWithError("sendto() отправил неверное количество байтов");

    // Установка обработчика сигнала SIGINT
    signal(SIGINT, HandleSigint);

    while (1) {
        // Получение сообщения от сервера
        if ((recvMsgSize = recvfrom(sock, buffer, BUFSIZE - 1, 0, NULL, NULL)) < 0)
            DieWithError("recvfrom() не удалось");
        
        buffer[recvMsgSize] = '\0';  // Добавление завершающего нуля к сообщению
        if (strcmp(buffer, TERMINATE_MSG) == 0) {
            printf("Сервер завершил работу\n");
            break;  // Выход из цикла при получении сообщения о завершении работы
        }
        printf("%s\n", buffer);  // Вывод полученного сообщения
    }

    close(sock);  // Закрытие сокета
    return 0;
}

// Функция обработки ошибок
void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

// Обработчик сигнала SIGINT
void HandleSigint(int sig) {
    printf("\nКлиент завершает работу\n");
    close(sock);  // Закрытие сокета
    exit(0);
}
