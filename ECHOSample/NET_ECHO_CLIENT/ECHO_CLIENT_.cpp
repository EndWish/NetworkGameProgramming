#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h> // 윈속2 메인 헤더
#include <ws2tcpip.h> // 윈속2 확장 헤더

#pragma comment(lib, "ws2_32") // ws2_32.lib 링크

using namespace std;

char* SERVERIP;
#define BUFSIZE    512

int main(int argc, char* argv[]) {
	int retval;

	// 명령행 인수가 있으면 IP 주소로 사용
	if (argc > 1) SERVERIP = argv[1];

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

	string domainName;
	cout << "도메인 네임을 입력하세요 : ";
	cin >> domainName;
	int portNumber;
	cout << "시작할 포트 번호를 입력하세요 : ";
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
			cout << "포트번호 " << portNumber + i << " : 실패\n";
			//err_quit("connect()");
		}
		else {
			cout << "포트번호 " << ntohs(serveraddr.sin_port) << " : 성공" << "\n";
			break;
		}
	}

	// 소켓 닫기
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}