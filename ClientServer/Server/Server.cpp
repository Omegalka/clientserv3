#define WIN32_LEAN_AND_MEAN 

#include <iostream>
#include <windows.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <stdexcept>
using namespace std;

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

class AccountHolder {
public:
    AccountHolder(const string& lastName, const string& firstName, int creditRating)
        : lastName(lastName), firstName(firstName), creditRating(creditRating) {}

private:
    string lastName;
    string firstName;
    int creditRating;
    chrono::system_clock::time_point registrationDate;
};

class Operation {
public:
    enum class Type { DEPOSIT, WITHDRAWAL };
    enum class Status { PENDING, COMPLETED, CANCELED };

    Operation(Type type) : type(type), timestamp(chrono::system_clock::now()), status(Status::PENDING) {}

    Status getStatus() const { return status; }
    void setStatus(Status status) { this->status = status; }

private:
    Type type;
    chrono::system_clock::time_point timestamp;
    Status status;
};

class Account {
public:
    Account(AccountHolder holder) : holder(holder), balance(0) {}

    void deposit(double amount) {
        lock_guard<mutex> lock(mtx);
        balance += amount;
    }

    void withdraw(double amount) {
        unique_lock<mutex> lock(mtx);
        if (balance < amount) {
            cv.wait_for(lock, chrono::seconds(3), [&] { return balance >= amount; });
            if (balance < amount) {
                throw runtime_error("Insufficient funds");
            }
        }
        balance -= amount;
    }

    void transfer(Account& recipient, double amount) {
        if (this == &recipient) {
            throw invalid_argument("Cannot transfer to the same account");
        }

        lock_guard<mutex> lockSender(mtx);
        lock_guard<mutex> lockRecipient(recipient.mtx);

        withdraw(amount);
        recipient.deposit(amount);
    }

private:
    AccountHolder holder;
    chrono::system_clock::time_point openDateTime;
    chrono::system_clock::time_point closeDateTime;
    double balance;
    vector<Operation> operations;
    mutex mtx;
    condition_variable cv;
};

void handleClient(SOCKET clientSocket) {
    // TODO: Додати обробку запитів від клієнта
}

int main() {
    setlocale(0, "");
    system("title SERVER SIDE");

    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed with error: " << iResult << "\n";
        cout << "подключение Winsock.dll прошло с ошибкой!\n";
        return 1;
    }

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* result = NULL;
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        cout << "getaddrinfo failed with error: " << iResult << "\n";
        cout << "получение адреса и порта сервера прошло c ошибкой!\n";
        WSACleanup();
        return 2;
    }

    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        cout << "socket failed with error: " << WSAGetLastError() << "\n";
        cout << "создание сокета прошло c ошибкой!\n";
        freeaddrinfo(result);
        WSACleanup();
        return 3;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        cout << "bind failed with error: " << WSAGetLastError() << "\n";
        cout << "внедрение сокета по IP-адресу прошло с ошибкой!\n";
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 4;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        cout << "listen failed with error: " << WSAGetLastError() << "\n";
        cout << "прослушка информации от клиента не началась. что-то пошло не так!\n";
        closesocket(ListenSocket);
        WSACleanup();
        return 5;
    }

    while (true) {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            cout << "accept failed with error: " << WSAGetLastError() << "\n";
            cout << "соединение с клиентским приложением не установлено\n";
            closesocket(ListenSocket);
            WSACleanup();
            return 6;
        }

        thread clientThread(handleClient, ClientSocket);
        clientThread.detach(); 

        cout << "Нове з'єднання виявлено. Очікуємо на клієнтські запити...\n";
    }

    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}
