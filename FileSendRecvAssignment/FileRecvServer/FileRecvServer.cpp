#include "..\Common.h"
#include <fstream>
#include <string>

#define SERVERPORT 9000
#define BUFSIZE    100'000

bool ErrorCheck(int retval);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf);
bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file);

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
	ULONG len; // 고정 길이 데이터
	char buf[BUFSIZE + 1]; // 가변 길이 데이터

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
		printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			addr, ntohs(clientaddr.sin_port));

		// 클라이언트와 데이터 통신
		// 1. 전달 받을 파일 이름 받아오기
		RcevFixedVariable(client_sock, len, buf);
		string fileName(buf, (size_t)len);
		ofstream out{ fileName, ios::binary};

		// 2. 파일 데이터 받기
		RcevFixedVariable(client_sock, len, buf, out);

		// 소켓 닫기
		closesocket(client_sock);
		printf("[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
			addr, ntohs(clientaddr.sin_port));
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

bool RcevFixedVariable(SOCKET& sock, ULONG& len, char* buf, ofstream& file) {
	int retval;

	// 파일의 크기를 받는다.(고정 길이)
	retval = recv(sock, (char*)&len, sizeof(ULONG), MSG_WAITALL);
	if (ErrorCheck(retval))
		return false;

	ULONG popSize = len;
	// 파일 데이터를 받는다.(가변 길이)
	while (true) {
		if (BUFSIZE < popSize) {
			retval = recv(sock, buf, BUFSIZE, MSG_WAITALL);
			if (ErrorCheck(retval))
				return false;
			file.write(buf, BUFSIZE);
			popSize -= BUFSIZE;
		}
		else {
			retval = recv(sock, buf, popSize, MSG_WAITALL);
			if (ErrorCheck(retval))
				return false;
			file.write(buf, popSize);
			popSize -= retval;
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