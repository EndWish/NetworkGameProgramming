#include <windows.h>
#include <stdio.h>

struct Point3D {
	int x, y, z;
};

DWORD WINAPI MyThread(LPVOID arg) {
	Sleep(1000);
	Point3D* pt = (Point3D*)arg;
	printf("Running MyThread() %d: %d, %d, %d\n",
		GetCurrentThreadId(), pt->x, pt->y, pt->z);
	return 0;
}

int main(int argc, char* argv[]) {
	// ù ��° ������ ����
	Point3D pt1 = { 10, 20, 30 };
	HANDLE hThread1 = CreateThread(NULL, 0, MyThread, &pt1, 0, NULL);
	if (hThread1 == NULL) return 1;
	//CloseHandle(hThread1);

	// �� ��° ������ ����
	Point3D pt2 = { 40, 50, 60 };
	HANDLE hThread2 = CreateThread(NULL, 0, MyThread, &pt2, 0, NULL);
	if (hThread2 == NULL) return 1;
	//CloseHandle(hThread2);

	printf("Running main() %d\n", GetCurrentThreadId());

	DWORD rst = WaitForSingleObject(hThread1, INFINITE);
	rst = WaitForSingleObject(hThread2, INFINITE);
	CloseHandle(hThread1);
	CloseHandle(hThread2);
	//Sleep(2000);
	return 0;
}