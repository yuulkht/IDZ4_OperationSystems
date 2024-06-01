#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define RCVBUFSIZE 256   // Размер буфера для приема сообщений
#define TERMINATE_MSG "SERVER_TERMINATE"

void DieWithError(char *errorMessage);
int** playGame(int numPlayers);
void HandleSigint(int sig);
int* getPointResults(int** results, int numPlayers);

int servSock;  // Сокет сервера
int observerSock;  // Сокет для наблюдателя
int numPlayers;  // Количество игроков
struct sockaddr_in* clientAddrs;  // Адреса клиентов
socklen_t* clientAddrLens;  // Длины адресов клиентов
struct sockaddr_in observerAddr;  // Адрес наблюдателя
socklen_t observerAddrLen;  // Длина адреса наблюдателя

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

    printf("Все игроки подключены. Ожидание подключения наблюдателя...\n");

    // Ожидание подключения наблюдателя
    observerAddrLen = sizeof(observerAddr);
    char observerStr[RCVBUFSIZE];
    if (recvfrom(servSock, observerStr, RCVBUFSIZE - 1, 0,
                 (struct sockaddr *) &observerAddr, &observerAddrLen) < 0)
        DieWithError("recvfrom() для наблюдателя не удалось");

    printf("Наблюдатель подключен.\n");

    printf("Начинается турнир...\n");

    int** results = playGame(numPlayers);  // Проведение игры

    // Отправка результатов игрокам
    for (int i = 0; i < numPlayers; i++) {
        int first_id = playerNumbers[i];
        for (int j = i + 1; j < numPlayers; j++) {
            int second_id = playerNumbers[j];
            char result_i[RCVBUFSIZE];
            char result_j[RCVBUFSIZE];
            char observer_result[RCVBUFSIZE];

            if (results[i][j] == 1) {
                sprintf(result_i, "Ваш результат против игрока %d: победа, очки: %d\n", second_id, 2);
                sprintf(result_j, "Ваш результат против игрока %d: поражение, очки: %d\n", first_id, 0);
                sprintf(observer_result, "Играет игрок %d против игрока %d: По итогам поединка игрок %d выиграл у игрока %d\n", first_id, second_id, first_id, second_id);
            } else if (results[i][j] == 2) {
                sprintf(result_i, "Ваш результат против игрока %d: поражение, очки: %d\n", second_id, 0);
                sprintf(result_j, "Ваш результат против игрока %d: победа, очки: %d\n", first_id, 2);
                sprintf(observer_result, "Играет игрок %d против игрока %d: По итогам поединка игрок %d выиграл у игрока %d\n", first_id, second_id, second_id, first_id);
            } else {
                sprintf(result_i, "Ваш результат против игрока %d: ничья, очки: %d\n", second_id, 1);
                sprintf(result_j, "Ваш результат против игрока %d: ничья, очки: %d\n", first_id, 1);
                sprintf(observer_result, "Играет игрок %d против игрока %d: По итогам поединка у игроков ничья\n", first_id, second_id);
            }

            // Отправка результатов первому игроку
            if (sendto(servSock, result_i, strlen(result_i), 0,
                       (struct sockaddr *) &clientAddrs[i], clientAddrLens[i]) != strlen(result_i))
                DieWithError("sendto() отправил неверное количество байтов");

            // Отправка результатов второму игроку
            if (sendto(servSock, result_j, strlen(result_j), 0,
                       (struct sockaddr *) &clientAddrs[j], clientAddrLens[j]) != strlen(result_j))
                DieWithError("sendto() отправил неверное количество байтов");

            // Отправка результатов наблюдателю
            if (sendto(servSock, observer_result, strlen(observer_result), 0,
                       (struct sockaddr *) &observerAddr, observerAddrLen) != strlen(observer_result))
                DieWithError("sendto() наблюдателю не удалось");
        }
    }

    int* points_results = getPointResults(results, numPlayers);

    for (int i = 0; i < numPlayers; i++) {
        int player_id = playerNumbers[i];
        char points_msg[RCVBUFSIZE];
        snprintf(points_msg, RCVBUFSIZE, "Игрок %d получает в итоге %d очков\n", player_id, points_results[i]);
        
        if (sendto(servSock, points_msg, strlen(points_msg), 0,
                (struct sockaddr *) &observerAddr, observerAddrLen) != strlen(points_msg))
            DieWithError("sendto() наблюдателю не удалось");
    }

    printf("Турнир завершен, результаты отправлены всем игрокам и наблюдателю\n");

    for (int i = 0; i < numPlayers; i++) {
        if (sendto(servSock, TERMINATE_MSG, strlen(TERMINATE_MSG), 0,
                   (struct sockaddr *) &clientAddrs[i], clientAddrLens[i]) != strlen(TERMINATE_MSG))
            DieWithError("sendto() отправил неверное количество байтов");
    }

    if (sendto(servSock, TERMINATE_MSG, strlen(TERMINATE_MSG), 0,
               (struct sockaddr *) &observerAddr, observerAddrLen) != strlen(TERMINATE_MSG))
        DieWithError("sendto() наблюдателю не удалось");

    // Освобождение выделенной памяти и закрытие сокета
    free(points_results);
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
            } else {
                results[i][j] = result;
                results[j][i] = (result == 1) ? 2 : (result == 2) ? 1 : 0;
            }
        }
    }

    return results;
}

// Получение общего количества очков, заработанных игроками
int* getPointResults(int** results, int numPlayers) {
    int* points_results = (int*)malloc(numPlayers * sizeof(int));
    if (points_results == NULL) {
        fprintf(stderr, "Ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < numPlayers; i++) {
        points_results[i] = 0;
        for (int j = 0; j < numPlayers; j++) {
            if (i != j) {
                if (results[i][j] == 1) {
                    points_results[i] += 2;
                }
                else if (results[i][j] == 0) {
                    points_results[i] += 1;
                }
            }
        }
    }
    return points_results;
}

// Функция обработки ошибок
void DieWithError(char *errorMessage) {
    perror(errorMessage);
    exit(1);
}

// Обработчик сигнала SIGINT
void HandleSigint(int sig) {
    printf("Сервер завершает работу...\n");

    for (int i = 0; i < numPlayers; i++) {
        if (sendto(servSock, TERMINATE_MSG, strlen(TERMINATE_MSG), 0,
                   (struct sockaddr *) &clientAddrs[i], clientAddrLens[i]) != strlen(TERMINATE_MSG))
            DieWithError("sendto() отправил неверное количество байтов");
    }

    if (sendto(servSock, TERMINATE_MSG, strlen(TERMINATE_MSG), 0,
               (struct sockaddr *) &observerAddr, observerAddrLen) != strlen(TERMINATE_MSG))
        DieWithError("sendto() наблюдателю не удалось");

    close(servSock);
    exit(0);
}
