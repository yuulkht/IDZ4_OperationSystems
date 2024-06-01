#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define RCVBUFSIZE 128   // Размер буфера для приема сообщений
#define TERMINATE_MSG "SERVER_TERMINATE"

void DieWithError(char *errorMessage);
int** playGame(int numPlayers);
void HandleSigint(int sig);

int servSock;  // Сокет сервера
int numPlayers;  // Количество игроков
struct sockaddr_in* clientAddrs;  // Адреса клиентов
socklen_t* clientAddrLens;  // Длины адресов клиентов

int main(int argc, char *argv[]) {
    struct sockaddr_in echoServAddr;  // Адрес сервера
    unsigned short echoServPort;  // Порт сервера
    unsigned int clientAddrLen;  // Длина адреса клиента

    // Проверка корректности количества аргументов командной строки
    if (argc != 3) {
        fprintf(stderr, "Использование: %s <Порт сервера> <Количество игроков>\n", argv[0]);
        exit(1);
    }

    echoServPort = atoi(argv[1]);  // Порт сервера из командной строки
    numPlayers = atoi(argv[2]);  // Количество игроков из командной строки

    int playerNumbers[numPlayers];  // Номера игроков

    signal(SIGINT, HandleSigint);  // Установка обработчика сигнала SIGINT

    // Выделение памяти для адресов и длин адресов клиентов
    clientAddrs = (struct sockaddr_in*)malloc(numPlayers * sizeof(struct sockaddr_in));
    clientAddrLens = (socklen_t*)malloc(numPlayers * sizeof(socklen_t));

    // Создание сокета
    if ((servSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() не удалось создать");

    // Заполнение структуры адреса сервера
    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);

    // Привязка сокета к локальному адресу
    if (bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() не удалось");

    printf("Ожидание подключения %d игроков...\n", numPlayers);

    // Ожидание подключения игроков
    for (int i = 0; i < numPlayers; i++) {
        clientAddrLens[i] = sizeof(clientAddrs[i]);
        char playerNumberStr[RCVBUFSIZE];
        int recvMsgSize;
        if ((recvMsgSize = recvfrom(servSock, playerNumberStr, RCVBUFSIZE - 1, 0,
                                    (struct sockaddr *) &clientAddrs[i], &clientAddrLens[i])) < 0)
            DieWithError("recvfrom() не удалось");
        playerNumberStr[recvMsgSize] = '\0';
        playerNumbers[i] = atoi(playerNumberStr);
        printf("Игрок %d подключен\n", playerNumbers[i]);
    }

    printf("Все игроки подключены. Начало турнира...\n");

    int** results = playGame(numPlayers);  // Проведение игры

    // Отправка результатов игрокам
    for (int i = 0; i < numPlayers; i++) {
        int first_id = playerNumbers[i];
        for (int j = i + 1; j < numPlayers; j++) {
            int second_id = playerNumbers[j];
            char result_i[RCVBUFSIZE];
            char result_j[RCVBUFSIZE];

            if (results[i][j] == 1) {
                sprintf(result_i, "Ваш результат против игрока %d: победа, очки: %d\n", second_id, 2);
                sprintf(result_j, "Ваш результат против игрока %d: поражение, очки: %d\n", first_id, 0);
            } else if (results[i][j] == 2) {
                sprintf(result_i, "Ваш результат против игрока %d: поражение, очки: %d\n", second_id, 0);
                sprintf(result_j, "Ваш результат против игрока %d: победа, очки: %d\n", first_id, 2);
            } else {
                sprintf(result_i, "Ваш результат против игрока %d: ничья, очки: %d\n", second_id, 1);
                sprintf(result_j, "Ваш результат против игрока %d: ничья, очки: %d\n", first_id, 1);
            }

            // Отправка результатов первому игроку
            if (sendto(servSock, result_i, strlen(result_i), 0,
                       (struct sockaddr *) &clientAddrs[i], clientAddrLens[i]) != strlen(result_i))
                DieWithError("sendto() отправил неверное количество байтов");

            // Отправка результатов второму игроку
            if (sendto(servSock, result_j, strlen(result_j), 0,
                       (struct sockaddr *) &clientAddrs[j], clientAddrLens[j]) != strlen(result_j))
                DieWithError("sendto() отправил неверное количество байтов");
        }
    }

    printf("Турнир завершен, результаты отправлены всем игрокам\n");

    for (int i = 0; i < numPlayers; i++) {
        if (sendto(servSock, TERMINATE_MSG, strlen(TERMINATE_MSG), 0,
                   (struct sockaddr *) &clientAddrs[i], clientAddrLens[i]) != strlen(TERMINATE_MSG))
            DieWithError("sendto() отправил неверное количество байтов");
    }

    // Освобождение выделенной памяти и закрытие сокета
    free(clientAddrs);
    free(clientAddrLens);
    close(servSock);
    return 0;
}

// Функция проведения игры
int** playGame(int numPlayers) {
    int** results = (int**)malloc(numPlayers * sizeof(int*));
    if (results == NULL) {
        fprintf(stderr, "Ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numPlayers; i++) {
        results[i] = (int*)malloc(numPlayers * sizeof(int));
        if (results[i] == NULL) {
            fprintf(stderr, "Ошибка выделения памяти\n");
            for (int j = 0; j < i; j++) {
                free(results[j]);
            }
            free(results);
            exit(EXIT_FAILURE);
        }
    }

    srand(time(0));
    for (int i = 0; i < numPlayers; i++) {
        for (int j = i; j < numPlayers; j++) {
            int result = rand() % 3;
            if (i == j) {
                results[i][j] = 3;  // Ничья с самим собой
            }
            if (result == 0) {
                results[i][j] = 0;  // Ничья
                results[j][i] = 0;
            }
            else if (result == 1) {
                results[i][j] = 1;  // Победа первого игрока
                results[j][i] = 2;  // Поражение второго игрока
            } else {
                results[i][j] = 2;  // Поражение первого игрока
                results[j][i] = 1;  // Победа второго игрока
            }
        }
    }

    return results;
}

// Функция обработки ошибки
void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

// Обработчик сигнала SIGINT
void HandleSigint(int sig) {
    printf("\nСервер завершает работу\n");

    // Отправка сообщения о завершении всем клиентам
    for (int i = 0; i < numPlayers; i++) {
        if (sendto(servSock, TERMINATE_MSG, strlen(TERMINATE_MSG), 0,
                   (struct sockaddr *) &clientAddrs[i], clientAddrLens[i]) != strlen(TERMINATE_MSG))
            DieWithError("sendto() отправил неверное количество байтов");
    }

    free(clientAddrs);
    free(clientAddrLens);
    close(servSock);
    exit(0);
}
