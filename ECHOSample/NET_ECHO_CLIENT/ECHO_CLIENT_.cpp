#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h> // ����2 ���� ���
#include <ws2tcpip.h> // ����2 Ȯ�� ���

#pragma comment(lib, "ws2_32") // ws2_32.lib ��ũ

using namespace std;

char* SERVERIP;
#define BUFSIZE    512

int main(int argc, char* argv[]) {
	int retval;

	// ����� �μ��� ������ IP �ּҷ� ���
	if (argc > 1) SERVERIP = argv[1];

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

	string domainName;
	cout << "������ ������ �Է��ϼ��� : ";
	cin >> domainName;
	int portNumber;
	cout << "������ ��Ʈ ��ȣ�� �Է��ϼ��� : ";
	cin >> portNumber;

	hostent* ptr = gethostbyname(domainName.c_str());
	SERVERIP = inet_ntoa(*(in_addr*)ptr->h_addr_list[0]);

	// connect()
	for (int i = 0; i < 5; ++i) {
		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr);
		serveraddr.sin_port = htons(portNumber + i);
		retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) {
			cout << "��Ʈ��ȣ " << portNumber + i << " : ����\n";
			//err_quit("connect()");
		}
		else {
			cout << "��Ʈ��ȣ " << ntohs(serveraddr.sin_port) << " : ����" << "\n";
			break;
		}
	}

	// ���� �ݱ�
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}