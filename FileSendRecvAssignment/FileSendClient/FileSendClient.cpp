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

	// ����� �μ��� ������ IP �ּҷ� ���
	if (argc > 1) SERVERIP = argv[1];

	string serverIP;
	cin >> serverIP;

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// ���� ����
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

	// ������ ������ �̸��� �Է¹޾� �о�´�.
	string fileName;
	cout << "������ ���� �̸��� �Է��Ͻÿ� : ";
	cin >> fileName;

	// ������ ��ſ� ����� ���� ����
	cout << "���۽� ����� ������ ����� �Է��Ͻÿ� : ";
	cin >> bufSize;
	char* buf = new char[bufSize];

	// ������ ������ ���
	auto startTime = std::chrono::system_clock::now();

	// 1. ���� �̸� ������
	SendFixedVariable(sock, fileName.length(), fileName.c_str(), buf);

	// 2. ���� ������ ������
	//SendFixedVariable(sock, fileSize, file., buf); -> ������ �����͸� �������� �ذ��ؾ��Ѵ�.
	size_t fileSize = filesystem::file_size(fileName);
	ifstream file{ fileName, ios::binary };	// c++ 17�̻�
	SendFixedVariable(sock, fileSize, file, buf);

	cout << bufSize << "ũ���� ���� ����� �����\n";
	auto endTime = std::chrono::system_clock::now();
	auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	cout << sec << " �� �ð��� �ɷȽ��ϴ�.\n";

	delete[] buf;

	// ���� �ݱ�
	closesocket(sock);

	// ���� ����
	WSACleanup();
	return 0;
}

bool SendFixedVariable(SOCKET& sock, ULONG pushSize, const char* src, char* buf) {
	int retval;

	// ������ ������(���� ����)
	if (!SendAndErrorCheck(sock, (char*)&pushSize, sizeof(ULONG), 0))
		return false;

	// ������ ������
	while (true) {
		if (bufSize < pushSize) {	// ���ۻ������ ���� �����Ͱ� Ŭ ���
			copy_n(src, bufSize, buf);
			if (!SendAndErrorCheck(sock, buf, bufSize, 0))
				return false;
			src += bufSize;
			pushSize -= bufSize;
		}
		else {	// ���������Ͱ� ���ۻ������ ���� ��� 
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

	// ������ ������(���� ����)
	if (!SendAndErrorCheck(sock, (char*)&pushSize, sizeof(ULONG), 0))
		return false;

	// ������ ������
	while (true) {
		if (bufSize < pushSize) {	// ���ۻ������ ���� �����Ͱ� Ŭ ���
			file.read(buf, bufSize);
			if (!SendAndErrorCheck(sock, buf, bufSize, 0))
				return false;
			pushSize -= bufSize;
		}
		else {	// ���������Ͱ� ���ۻ������ ���� ��� 
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