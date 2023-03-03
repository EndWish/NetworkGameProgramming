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

	int fileSize;	// 보낼 파일의 크기
	int sendSize;	// 보낸 파일의 크기

public:
	SESSION() = default;
	SESSION(long long _id, SOCKET client_sock) : id{ _id }, sock{ client_sock } {
		struct sockaddr_in clientaddr;
		int addrlen;
		addrlen = sizeof(clientaddr);
		getpeername(sock, (struct sockaddr*)&clientaddr, &addrlen);
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));	// 클라이언트의 주소정보를 저장
		portNumber = ntohs(clientaddr.sin_port);	// 클라이언트의 포트번호를 저장

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

	// 함수
	void Print() {
		printf("[TCP 서버] 클라이언트 접속중: IP 주소=%s, 포트 번호=%d\n", addr, portNumber);
		if (fileSize > 0) {	// 전송할 파일이 존재
			long long rate = (long long)sendSize * 100 / (long long)fileSize;
			cout << "파일 전송률 : (";
			for (int i = 1; i <= 20; ++i) {
				if (i * 5 <= rate)
					cout << "■";
				else
					cout << "□";
			}
			cout << ") " << sendSize << "/" << fileSize;

			if (rate >= 100) {
				cout << " <전송완료>";
			}
			else {
				cout << ", " << rate << "%";
			}

			cout << "\n";
		}
		else {
			printf("대기중...\n");
		}
	}
};

unordered_map<long long, SESSION> clients;

int idCount;

void Print();
bool ErrorCheck(int retval);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file, int clientID);

// 클라이언트와 데이터 통신

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

	// 클라이언트 정보 얻기
	//addrlen = sizeof(clientaddr);
	//getpeername(client_sock, (struct sockaddr*)&clientaddr, &addrlen);
	//inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));

	while (true) {
		// 클라이언트와 데이터 통신
		// 1. 전달 받을 파일 이름 받아오기
		if(!RcevFixedVariable(client_sock, len, buf))
			break;
		string fileName(buf, (size_t)len);
		ofstream out{ fileName, ios::binary };

		// 2. 파일 데이터 받기
		if (!RcevFixedVariable(client_sock, len, buf, out, id))
			break;
	}

	// 소켓 닫기
	//closesocket(client_sock);
	//printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
	//	addr, ntohs(clientaddr.sin_port));

	clients.erase(id);

	return 0;
}

int main(int argc, char* argv[]) {
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
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

	// 데이터 통신에 사용할 변수
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
		
		// 접속한 클라이언트 정보 출력
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n", addr, ntohs(clientaddr.sin_port));

		// 스레드 생성
		int newID = idCount++;
		clients.try_emplace(newID, newID, client_sock);
		hThread = CreateThread(NULL, 0, ProcessClient, (LPVOID)newID, 0, NULL);
		if (hThread == NULL) { closesocket(client_sock); }
		else { CloseHandle(hThread); }
	}

	// 소켓 닫기
	closesocket(listen_sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf) {
	int retval;

	// 데이터 받기(고정 길이)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	// 데이터 받기(가변 길이)
	retval = recv(sock, buf, len, MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	return true;
}

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file, int clientID) {
	int retval;

	// 파일의 크기를 받는다.(고정 길이)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	clients[clientID].SetFileSize(len);

	ULONG popSize = len;
	// 파일 데이터를 받는다.(가변 길이)
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
		cout << "접속한 클라이언트가 없습니다.\n";
	}

	for (auto [id, client] : clients) {
		client.Print();
		cout << "\n";
	}
}
