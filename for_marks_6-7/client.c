#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define RCVBUFSIZE 256  // Размер буфера для приема сообщений
#define TERMINATE_MSG "SERVER_TERMINATE"

void DieWithError(char *errorMessage);  // Объявление функции обработки ошибок
void HandleSigint(int sig);  // Объявление обработчика сигнала

int sock;  // Сокет

int main(int argc, char *argv[]) {
    struct sockaddr_in echoServAddr;  // Адрес сервера
    unsigned short echoServPort;  // Порт сервера
    char *servIP;  // IP-адрес сервера
    char *playerNumber;  // Номер игрока
    char echoBuffer[RCVBUFSIZE];  // Буфер для приема сообщений
    int bytesRcvd;  // Количество полученных байтов за одно
    int sumPoints = 0;  // Суммарное количество очков

    // Проверка количества аргументов командной строки
    if (argc != 4) {
        fprintf(stderr, "Использование: %s <IP сервера> <Порт сервера> <Номер игрока>\n", argv[0]);
        exit(1);
    }

    servIP = argv[1];  // IP-адрес сервера
    echoServPort = atoi(argv[2]);  // Порт сервера
    playerNumber = argv[3];  // Номер игрока

    signal(SIGINT, HandleSigint);  // Установка обработчика сигнала SIGINT

    // Создание сокета
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() не удалось создать");

    // Заполнение структуры адреса сервера
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);

    struct sockaddr_in fromAddr;  // Адрес отправителя
    unsigned int fromSize = sizeof(fromAddr);  // Размер адреса отправителя

    // Отправка номера игрока серверу
    if (sendto(sock, playerNumber, strlen(playerNumber), 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) != strlen(playerNumber))
        DieWithError("sendto() отправил неверное количество байтов");

    printf("Ожидание сообщений от сервера...\n");

    // Прием сообщений от сервера
    while (1) {
        bytesRcvd = recvfrom(sock, echoBuffer, RCVBUFSIZE - 1, 0, (struct sockaddr *) &fromAddr, &fromSize);
        if (bytesRcvd < 0)
            DieWithError("recvfrom() не удалось");

        echoBuffer[bytesRcvd] = '\0';  // Добавление нуль-терминатора к строке

        // Проверка на сообщение о завершении сервера
        if (strcmp(echoBuffer, TERMINATE_MSG) == 0) {
            break;
        }

        // Извлечение результата из последнего символа строки
        char lastChar = echoBuffer[strlen(echoBuffer) - 2];
        int result = lastChar - '0';
        sumPoints += result;

        printf("Получено: %s", echoBuffer);
    }

    printf("Ваши общие очки: %d\n", sumPoints);

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
    close(sock);
    exit(0);
}
