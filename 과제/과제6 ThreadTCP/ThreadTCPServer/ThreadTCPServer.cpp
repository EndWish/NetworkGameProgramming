#include "..\Common.h"
#include <fstream>
#include <string>
#include <unordered_map>

#define SERVERPORT 9000
#define BUFSIZE    10'000

class SESSION {
private:
	long long id;
	SOCKET sock;
	char addr[INET_ADDRSTRLEN];
	USHORT portNumber;

	int fileSize;	// ���� ������ ũ��
	int sendSize;	// ���� ������ ũ��

public:
	SESSION() = default;
	SESSION(long long _id, SOCKET client_sock) : id{ _id }, sock{ client_sock } {
		struct sockaddr_in clientaddr;
		int addrlen;
		addrlen = sizeof(clientaddr);
		getpeername(sock, (struct sockaddr*)&clientaddr, &addrlen);
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));	// Ŭ���̾�Ʈ�� �ּ������� ����
		portNumber = ntohs(clientaddr.sin_port);	// Ŭ���̾�Ʈ�� ��Ʈ��ȣ�� ����

		fileSize = 0;
		sendSize = 0;
	}

	// get
	int GetFileSize() const {
		return fileSize;
	}
	int GetSendSize() const {
		return sendSize;
	}
	SOCKET& GetSocket() {
		return sock;
	}

	// set
	void SetFileSize(int _fileSize) {
		fileSize = _fileSize;
	}
	void SetSendSize(int _sendSize) {
		sendSize = _sendSize;
	}

	// �Լ�
	void Print() {
		printf("[TCP ����] Ŭ���̾�Ʈ ������: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", addr, portNumber);
		if (fileSize > 0) {	// ������ ������ ����
			long long rate = (long long)sendSize * 100 / (long long)fileSize;
			cout << "���� ���۷� : (";
			for (int i = 1; i <= 20; ++i) {
				if (i * 5 <= rate)
					cout << "��";
				else
					cout << "��";
			}
			cout << ") " << sendSize << "/" << fileSize;

			if (rate >= 100) {
				cout << " <���ۿϷ�>";
			}
			else {
				cout << ", " << rate << "%";
			}

			cout << "\n";
		}
		else {
			printf("�����...\n");
		}
	}
};

unordered_map<long long, SESSION> clients;

int idCount;

void Print();
bool ErrorCheck(int retval);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file, int clientID);

// Ŭ���̾�Ʈ�� ������ ���

DWORD WINAPI Draw(PVOID arg) {
	while (true) {
		Print();
		Sleep(500);
	}
	return 0;
}

DWORD WINAPI ProcessClient(LPVOID arg) {
	int id = (int)arg;
	SOCKET& client_sock = clients[id].GetSocket();

	int retval;
	char buf[BUFSIZE + 1];
	ULONG len;

	// Ŭ���̾�Ʈ ���� ���
	//addrlen = sizeof(clientaddr);
	//getpeername(client_sock, (struct sockaddr*)&clientaddr, &addrlen);
	//inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));

	while (true) {
		// Ŭ���̾�Ʈ�� ������ ���
		// 1. ���� ���� ���� �̸� �޾ƿ���
		if(!RcevFixedVariable(client_sock, len, buf))
			break;
		string fileName(buf, (size_t)len);
		ofstream out{ fileName, ios::binary };

		// 2. ���� ������ �ޱ�
		if (!RcevFixedVariable(client_sock, len, buf, out, id))
			break;
	}

	// ���� �ݱ�
	//closesocket(client_sock);
	//printf("[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
	//	addr, ntohs(clientaddr.sin_port));

	clients.erase(id);

	return 0;
}

int main(int argc, char* argv[]) {
	int retval;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// ������ ��ſ� ����� ����
	SOCKET client_sock;
	struct sockaddr_in clientaddr;
	int addrlen;
	HANDLE hThread;

	CreateThread(NULL, 0, Draw, (LPVOID)NULL, 0, NULL);

	while (1) {
		// accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("accept()");
			break;
		}
		
		// ������ Ŭ���̾�Ʈ ���� ���
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n", addr, ntohs(clientaddr.sin_port));

		// ������ ����
		int newID = idCount++;
		clients.try_emplace(newID, newID, client_sock);
		hThread = CreateThread(NULL, 0, ProcessClient, (LPVOID)newID, 0, NULL);
		if (hThread == NULL) { closesocket(client_sock); }
		else { CloseHandle(hThread); }
	}

	// ���� �ݱ�
	closesocket(listen_sock);

	// ���� ����
	WSACleanup();
	return 0;
}

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf) {
	int retval;

	// ������ �ޱ�(���� ����)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	// ������ �ޱ�(���� ����)
	retval = recv(sock, buf, len, MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	return true;
}

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file, int clientID) {
	int retval;

	// ������ ũ�⸦ �޴´�.(���� ����)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	clients[clientID].SetFileSize(len);

	ULONG popSize = len;
	// ���� �����͸� �޴´�.(���� ����)
	while (true) {
		if (BUFSIZE < popSize) {
			retval = recv(sock, buf, BUFSIZE, MSG_WAITALL);
			if (ErrorCheck(retval))
				return false;
			file.write(buf, BUFSIZE);
			popSize -= BUFSIZE;
			clients[clientID].SetSendSize(len - popSize);
		}
		else {
			retval = recv(sock, buf, popSize, MSG_WAITALL);
			if (ErrorCheck(retval))
				return false;
			file.write(buf, popSize);
			popSize -= retval;
			clients[clientID].SetSendSize(len - popSize);
			break;
		}
	}

	return true;
}

bool ErrorCheck(int retval) {
	if (retval == SOCKET_ERROR) {
		err_display("recv()");
		return true;
	}
	else if (retval == 0)
		return true;

	return false;
}

void Print() {
	system("cls");

	if (clients.size() == 0) {
		cout << "������ Ŭ���̾�Ʈ�� �����ϴ�.\n";
	}

	for (auto [id, client] : clients) {
		client.Print();
		cout << "\n";
	}
}
