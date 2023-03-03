//#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")    // �ܼ�â ����( �׽�Ʈ�� ���� �뵵 )

#include "..\Common.h"
#include "resource.h"


#include <fstream>
#include <filesystem>	// c++17 �̻�
#include <string>

#include <commctrl.h>

#pragma comment(lib,"urlmon.lib")
#pragma comment(lib,"wininet.lib")

#define SERVERIP   "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE    1024

bool SendAndErrorCheck(SOCKET sock, const char* buf, int len, int flags);
bool SendFixedVariable(SOCKET& sock, ULONG pushSize, const char* src, char* buf);
bool SendFixedVariable(SOCKET& sock, ULONG pushSize, ifstream& file, char* buf);
void UpdateProgress();

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// ����Ʈ ��Ʈ�� ��� �Լ�
void DisplayText(const char *fmt, ...);
// ���� �Լ� ���� ���
void DisplayError(const char *msg);
// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);

SOCKET sock; // ����
char buf[BUFSIZE + 1]; // ������ �ۼ��� ����
HANDLE hReadEvent, hWriteEvent; // �̺�Ʈ
HWND hSearch; // ���� ã�� ��ư
HWND hPathBox; // ���� ��� ǥ��
HWND hSendButton; // ������ ��ư
HWND hProgressBar; // ����� ��

// ���Ͽ��� ��ȭ����
HWND hButtonOpenFileDialog; // ���Ͽ��� ��ȭ���ڸ� �����ϱ� ���� ��ư�� �ڵ�
HWND hEditFileToBeOpened; // ������ ��ο� �̸��� �������� ����Ʈ ��Ʈ���� �ڵ�
OPENFILENAME OFN; // ���Ͽ��� ��ȭ���ڸ� �ʱ�ȭ�ϱ� ���� ����
const UINT nFileNameMaxLen = 2048; // ���� �ٿ� �����ϴ� szFileName ���ڿ��� �ִ� ����
WCHAR szFileName[nFileNameMaxLen]; // ������ ��� �� �̸��� �����ϱ� ���� ���ڿ�

// ���ۻ�Ȳ�� ��Ÿ���� ���� ����
size_t fileSize;
size_t sendSize;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	setlocale(LC_ALL, "");

	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// �̺�Ʈ ����
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// ���� ��� ������ ����
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	// ��ȭ���� ����
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		hSearch = GetDlgItem(hDlg, IDC_BUTTON_SEARCH);

		hPathBox = GetDlgItem(hDlg, IDC_EDIT_PATH);

		hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS1);
		SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));		// ������� ���� ����
		SendMessage(hProgressBar, PBM_SETSTEP, (WPARAM)0, 0);
		SendMessage(hProgressBar, PBM_STEPIT, 0, 0);
		
		hSendButton = GetDlgItem(hDlg, IDOK);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON_SEARCH:
			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hDlg;
			OFN.lpstrFilter = L"All Files(*.*)\0*.*\0";
			OFN.lpstrFile = szFileName;
			OFN.nMaxFile = nFileNameMaxLen;
			if (GetOpenFileName(&OFN)) {
				SetWindowText(hEditFileToBeOpened, OFN.lpstrFile);
				SetWindowText(hPathBox, OFN.lpstrFile);
			}
			return TRUE;
		case IDOK:
			EnableWindow(hSendButton, FALSE); // ������ ��ư ��Ȱ��ȭ
			EnableWindow(hSearch, FALSE); // ������ ��ư ��Ȱ��ȭ
			WaitForSingleObject(hReadEvent, INFINITE); // �б� �Ϸ� ���
			SetEvent(hWriteEvent); // ���� �Ϸ� �˸�
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL); // ��ȭ���� �ݱ�
			closesocket(sock); // ���� �ݱ�
			return TRUE;
		case IDC_PROGRESS1:
			
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// TCP Ŭ���̾�Ʈ ���� �κ�
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	// ���� ����
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	// ������ ������ ���
	while (true) {
		WaitForSingleObject(hWriteEvent, INFINITE); // ���� �Ϸ� ���

		wstring filePath = OFN.lpstrFile;
		ifstream file{ filePath, ios::binary };	// c++ 17�̻�
		if (!file) {
			EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
			EnableWindow(hSearch, TRUE); // ������ ��ư Ȱ��ȭ
			SetEvent(hReadEvent); // �б� �Ϸ� �˸�
			continue;
		}
		fileSize = filesystem::file_size(filePath);
		sendSize = 0;

		// 1. ���� �̸� ������
		wstring fileName = filePath.substr( filePath.rfind('\\') + 1);
		SendFixedVariable(sock, fileName.length() * sizeof(WCHAR), (char*)fileName.data(), buf);

		// 2. ���� ������ ������
		SendFixedVariable(sock, fileSize, file, buf);

		EnableWindow(hSendButton, TRUE); // ������ ��ư Ȱ��ȭ
		EnableWindow(hSearch, TRUE); // ������ ��ư Ȱ��ȭ
		SetEvent(hReadEvent); // �б� �Ϸ� �˸�
	}

	return 0;
}

bool SendFixedVariable(SOCKET& sock, ULONG pushSize, const char* src, char* buf) {
	int retval;
	
	// ������ ������(���� ����)
	if (!SendAndErrorCheck(sock, (char*)&pushSize, sizeof(ULONG), 0))
		return false;

	// ������ ������
	while (true) {
		if (BUFSIZE < pushSize) {	// ���ۻ������ ���� �����Ͱ� Ŭ ���
			copy_n(src, BUFSIZE, buf);
			if (!SendAndErrorCheck(sock, buf, BUFSIZE, 0))
				return false;
			src += BUFSIZE;
			pushSize -= BUFSIZE;
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
		if (BUFSIZE < pushSize) {	// ���ۻ������ ���� �����Ͱ� Ŭ ���
			file.read(buf, BUFSIZE);
			if (!SendAndErrorCheck(sock, buf, BUFSIZE, 0))
				return false;
			pushSize -= BUFSIZE;
			sendSize += BUFSIZE;
			UpdateProgress();
		}
		else {	// ���������Ͱ� ���ۻ������ ���� ��� 
			file.read(buf, pushSize);
			if (!SendAndErrorCheck(sock, buf, pushSize, 0))
				return false;
			sendSize += pushSize;
			UpdateProgress();
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

void UpdateProgress() {
	double percent = (double)sendSize * 100 / fileSize;
	SendMessage(hProgressBar, PBM_SETPOS, (WPARAM)percent, 0);
	SendMessage(hProgressBar, PBM_STEPIT, 0, 0);
}
