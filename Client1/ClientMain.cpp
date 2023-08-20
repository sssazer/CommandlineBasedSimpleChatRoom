#include<iostream>
#include<winsock2.h>
#include<ws2tcpip.h>
#include<tchar.h>
#include<windows.h>
#include<locale>
#include<codecvt>
#include<signal.h>

using namespace std;

SOCKET clientSocket;

SOCKET createClient(string IPaddr, int port) {
    // Step1 - LoadDLL
    WSADATA wsaData;
    int wsaerr;
    WORD wVersionRequested = MAKEWORD(2, 2); // �汾2.2
    wsaerr = WSAStartup(wVersionRequested, &wsaData);
    if (wsaerr != 0) { // ���ط�0˵����ʼ��ʧ��
        cout << "The Winsock dll not found!" << endl;
        return 0;
    }
    else { // ��ʼ���ɹ�
        cout << "The Winsock dll found!" << endl;
        cout << "The status:" << wsaData.szSystemStatus << endl; // ��ӡ��ǰ״̬
    }

    // Step2 - CreateSocket
    clientSocket = INVALID_SOCKET;
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) { // ����ʧ��
        cout << "Error at socket():" << WSAGetLastError() << endl;
        WSACleanup(); // ����ʧ��Ҫ�ͷŵ���WinSock��ռ��
        return 0;
    }
    else {
        cout << "socket() is OK!" << endl;
    }

    // Step3 - Connect
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    wstring IPaddrTemp = wstring(IPaddr.begin(), IPaddr.end());
    InetPton(AF_INET, IPaddrTemp.c_str(), &clientService.sin_addr.s_addr);
    clientService.sin_port = htons(port); // �������˿ں�Ϊ55555
    if (connect(clientSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) { // ����ʧ��
        cout << "Client: connent() - Failed to connect." << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }
    else {
        cout << "Client: connect() is OK." << endl;
        cout << "Client: Can start sending and receiving data..." << endl;
    }
    return clientSocket;
}

bool sendFlag = true;

int sendMessage() {
    bool flag = true;
    while (sendFlag) {
        if (flag) {
            cout << "��ӭ���������ң������������ǳƣ�";
            flag = false;
        }
        // Step3 - Chat to Server
        char buffer[200];
        //printf("Enter your message(����q�ر�): ");
        cin.getline(buffer, 200);
        if (!strcmp(buffer, "q") || !strcmp(buffer, "Q")) { // ����q�˳�����
            closesocket(clientSocket);
            break;
        }
        int byteCount = send(clientSocket, buffer, sizeof(buffer), MSG_DONTROUTE);
        if (byteCount == SOCKET_ERROR) {
            printf("Server send error %ld.\n", WSAGetLastError());
            return -1;
        }
    }
    return 0;
}

bool recvFlag = true;

int receiveMessage() {

    while (recvFlag) {

        // Receive Response from server
        char receiveBuffer[200] = "";
        int byteReceived = recv(clientSocket, receiveBuffer, sizeof(receiveBuffer), 0);
        //string buffer_gb = UTF8ToGBK(receiveBuffer);
        if (byteReceived == SOCKET_ERROR) {
            cout << "����ʧ�ܣ�������룺" << WSAGetLastError() << endl;
            break;
        }
        else {
            cout << receiveBuffer << endl;
        }

    }
    return 0;

}


int receiveThreadCallback(void* pParam) {
    receiveMessage();
    return 0;
}

DWORD WINAPI sendThreadCallback(PVOID pParam) {
    sendMessage();
    return 0;
}


void sigint_handler(int sig) {
    if (sig == SIGINT) {
        closesocket(clientSocket);
        WSACleanup();
        sendFlag = false;
        recvFlag = false;
        std::cout << "�˳�������" << endl;
    }
}

int main() {
    signal(SIGINT, sigint_handler);
    SOCKET clientSocket = createClient("58.87.96.200", 55555);
    HANDLE receive_h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receiveThreadCallback, 0, 0, 0);
    HANDLE send_h = CreateThread(NULL, 0, sendThreadCallback, NULL, 0, 0);

    WaitForSingleObject(receive_h, INFINITE);
    WaitForSingleObject(send_h, INFINITE);

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}