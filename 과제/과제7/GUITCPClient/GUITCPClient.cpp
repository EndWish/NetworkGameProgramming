//#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")    // 콘솔창 띄우기( 테스트를 위한 용도 )

#include "..\Common.h"
#include "resource.h"


#include <fstream>
#include <filesystem>	// c++17 이상
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

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// 에디트 컨트롤 출력 함수
void DisplayText(const char *fmt, ...);
// 소켓 함수 오류 출력
void DisplayError(const char *msg);
// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);

SOCKET sock; // 소켓
char buf[BUFSIZE + 1]; // 데이터 송수신 버퍼
HANDLE hReadEvent, hWriteEvent; // 이벤트
HWND hSearch; // 파일 찾기 버튼
HWND hPathBox; // 파일 경로 표시
HWND hSendButton; // 보내기 버튼
HWND hProgressBar; // 진행률 바

// 파일열기 대화상자
HWND hButtonOpenFileDialog; // 파일열기 대화상자를 실행하기 위한 버튼의 핸들
HWND hEditFileToBeOpened; // 파일의 경로와 이름을 가져오는 에디트 컨트롤의 핸들
OPENFILENAME OFN; // 파일열기 대화상자를 초기화하기 위한 변수
const UINT nFileNameMaxLen = 2048; // 다음 줄에 정의하는 szFileName 문자열의 최대 길이
WCHAR szFileName[nFileNameMaxLen]; // 파일의 경로 및 이름을 복사하기 위한 문자열

// 전송상황을 나타내기 위한 변수
size_t fileSize;
size_t sendSize;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	setlocale(LC_ALL, "");

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 이벤트 생성
	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// 소켓 통신 스레드 생성
	CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);

	// 대화상자 생성
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 이벤트 제거
	CloseHandle(hReadEvent);
	CloseHandle(hWriteEvent);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		hSearch = GetDlgItem(hDlg, IDC_BUTTON_SEARCH);

		hPathBox = GetDlgItem(hDlg, IDC_EDIT_PATH);

		hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS1);
		SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));		// 진행바의 범위 설정
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
			EnableWindow(hSendButton, FALSE); // 보내기 버튼 비활성화
			EnableWindow(hSearch, FALSE); // 보내기 버튼 비활성화
			WaitForSingleObject(hReadEvent, INFINITE); // 읽기 완료 대기
			SetEvent(hWriteEvent); // 쓰기 완료 알림
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL); // 대화상자 닫기
			closesocket(sock); // 소켓 닫기
			return TRUE;
		case IDC_PROGRESS1:
			
			return TRUE;
		}
		return FALSE;
	}
	return FALSE;
}

// TCP 클라이언트 시작 부분
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	// 소켓 생성
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

	// 서버와 데이터 통신
	while (true) {
		WaitForSingleObject(hWriteEvent, INFINITE); // 쓰기 완료 대기

		wstring filePath = OFN.lpstrFile;
		ifstream file{ filePath, ios::binary };	// c++ 17이상
		if (!file) {
			EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
			EnableWindow(hSearch, TRUE); // 보내기 버튼 활성화
			SetEvent(hReadEvent); // 읽기 완료 알림
			continue;
		}
		fileSize = filesystem::file_size(filePath);
		sendSize = 0;

		// 1. 파일 이름 보내기
		wstring fileName = filePath.substr( filePath.rfind('\\') + 1);
		SendFixedVariable(sock, fileName.length() * sizeof(WCHAR), (char*)fileName.data(), buf);

		// 2. 파일 데이터 보내기
		SendFixedVariable(sock, fileSize, file, buf);

		EnableWindow(hSendButton, TRUE); // 보내기 버튼 활성화
		EnableWindow(hSearch, TRUE); // 보내기 버튼 활성화
		SetEvent(hReadEvent); // 읽기 완료 알림
	}

	return 0;
}

bool SendFixedVariable(SOCKET& sock, ULONG pushSize, const char* src, char* buf) {
	int retval;
	
	// 데이터 보내기(고정 길이)
	if (!SendAndErrorCheck(sock, (char*)&pushSize, sizeof(ULONG), 0))
		return false;

	// 데이터 보내기
	while (true) {
		if (BUFSIZE < pushSize) {	// 버퍼사이즈보다 보낼 데이터가 클 경우
			copy_n(src, BUFSIZE, buf);
			if (!SendAndErrorCheck(sock, buf, BUFSIZE, 0))
				return false;
			src += BUFSIZE;
			pushSize -= BUFSIZE;
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
		if (BUFSIZE < pushSize) {	// 버퍼사이즈보다 보낼 데이터가 클 경우
			file.read(buf, BUFSIZE);
			if (!SendAndErrorCheck(sock, buf, BUFSIZE, 0))
				return false;
			pushSize -= BUFSIZE;
			sendSize += BUFSIZE;
			UpdateProgress();
		}
		else {	// 보낼데이터가 버퍼사이즈보다 작을 경우 
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
