#include "..\Common.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>

char* SERVERIP = (char*)"127.0.0.1";
#define SERVERPORT 9000

ULONG bufSize;

bool SendAndErrorCheck(SOCKET sock, const char* buf, int len, int flags);
bool SendFixedVariable(SOCKET& sock, ULONG pushSize, const char* src, char* buf);
bool SendFixedVariable(SOCKET& sock, ULONG pushSize, ifstream& file, char* buf);

int main(int argc, char* argv[]) {
	int retval;

	// 명령행 인수가 있으면 IP 주소로 사용
	if (argc > 1) SERVERIP = argv[1];

	string serverIP;
	cin >> serverIP;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 소켓 생성
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, serverIP.c_str(), &serveraddr.sin_addr);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	// 전송할 파일의 이름을 입력받아 읽어온다.
	string fileName;
	cout << "전송할 파일 이름을 입력하시오 : ";
	cin >> fileName;

	// 데이터 통신에 사용할 버퍼 생성
	cout << "전송시 사용할 버퍼의 사이즈를 입력하시오 : ";
	cin >> bufSize;
	char* buf = new char[bufSize];

	// 서버와 데이터 통신
	auto startTime = std::chrono::system_clock::now();

	// 1. 파일 이름 보내기
	SendFixedVariable(sock, fileName.length(), fileName.c_str(), buf);

	// 2. 파일 데이터 보내기
	//SendFixedVariable(sock, fileSize, file., buf); -> 파일의 데이터를 보낼때를 해결해야한다.
	size_t fileSize = filesystem::file_size(fileName);
	ifstream file{ fileName, ios::binary };	// c++ 17이상
	SendFixedVariable(sock, fileSize, file, buf);

	cout << bufSize << "크기의 버퍼 사이즈를 사용결과\n";
	auto endTime = std::chrono::system_clock::now();
	auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	cout << sec << " 의 시간이 걸렸습니다.\n";

	delete[] buf;

	// 소켓 닫기
	closesocket(sock);

	// 윈속 종료
	WSACleanup();
	return 0;
}

bool SendFixedVariable(SOCKET& sock, ULONG pushSize, const char* src, char* buf) {
	int retval;

	// 데이터 보내기(고정 길이)
	if (!SendAndErrorCheck(sock, (char*)&pushSize, sizeof(ULONG), 0))
		return false;

	// 데이터 보내기
	while (true) {
		if (bufSize < pushSize) {	// 버퍼사이즈보다 보낼 데이터가 클 경우
			copy_n(src, bufSize, buf);
			if (!SendAndErrorCheck(sock, buf, bufSize, 0))
				return false;
			src += bufSize;
			pushSize -= bufSize;
		}
		else {	// 보낼데이터가 버퍼사이즈보다 작을 경우 
			copy_n(src, pushSize, buf);
			if (!SendAndErrorCheck(sock, buf, pushSize, 0))
				return false;
			break;
		}
	}

	return true;
}

bool SendFixedVariable(SOCKET& sock, ULONG pushSize, ifstream& file, char* buf) {
	int retval;

	// 데이터 보내기(고정 길이)
	if (!SendAndErrorCheck(sock, (char*)&pushSize, sizeof(ULONG), 0))
		return false;

	// 데이터 보내기
	while (true) {
		if (bufSize < pushSize) {	// 버퍼사이즈보다 보낼 데이터가 클 경우
			file.read(buf, bufSize);
			if (!SendAndErrorCheck(sock, buf, bufSize, 0))
				return false;
			pushSize -= bufSize;
		}
		else {	// 보낼데이터가 버퍼사이즈보다 작을 경우 
			file.read(buf, pushSize);
			if (!SendAndErrorCheck(sock, buf, pushSize, 0))
				return false;
			break;
		}
	}

	return true;
}

bool SendAndErrorCheck(SOCKET sock, const char* buf, int len, int flags) {
	int retval = send(sock, buf, len, flags);
	if (retval == SOCKET_ERROR) {
		err_display("send()");
		return false;
	}
	return true;
}