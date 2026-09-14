#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include "resource.h"

static INT_PTR CALLBACK
ErrorTestDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APPICON));
            if (hIcon)
            {
                SendMessageW(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessageW(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }
            return TRUE;
        }

        case WM_COMMAND:
        {
            WORD wId = LOWORD(wParam);
            if (wId == IDC_BTN_STARTUP)
            {
                SetDlgItemTextW(hwndDlg, IDC_STATUS_TEXT, L"Воспроизведение звука запуска KeshOS...");
                if (!PlaySoundW(L"SystemStart", NULL, SND_ALIAS | SND_ASYNC))
                {
                    WCHAR szPath[MAX_PATH];
                    ExpandEnvironmentStringsW(L"%SystemRoot%\\media\\KeshOS_Startup.wav", szPath, MAX_PATH);
                    PlaySoundW(szPath, NULL, SND_FILENAME | SND_ASYNC);
                }
                return TRUE;
            }
            else if (wId == IDC_BTN_ERROR)
            {
                WCHAR szPath[MAX_PATH];
                SetDlgItemTextW(hwndDlg, IDC_STATUS_TEXT, L"Вызов критической ошибки...");
                ExpandEnvironmentStringsW(L"%SystemRoot%\\media\\KeshOS_Error.wav", szPath, MAX_PATH);
                PlaySoundW(szPath, NULL, SND_FILENAME | SND_ASYNC);
                MessageBoxW(hwndDlg,
                            L"Тестовое системное уведомление!\n\nЗвук ошибки успешно воспроизведён.\nKeshOS NT 0.8.0 Beta \"Brownie\"",
                            L"KeshOS Brownie — Критическая ошибка",
                            MB_ICONERROR | MB_OK);
                SetDlgItemTextW(hwndDlg, IDC_STATUS_TEXT, L"Ошибка успешно обработана");
                return TRUE;
            }
            else if (wId == IDC_BTN_NOTIFY)
            {
                WCHAR szPath[MAX_PATH];
                SetDlgItemTextW(hwndDlg, IDC_STATUS_TEXT, L"Звук уведомления...");
                ExpandEnvironmentStringsW(L"%SystemRoot%\\media\\ReactOS_Notify.wav", szPath, MAX_PATH);
                PlaySoundW(szPath, NULL, SND_FILENAME | SND_ASYNC);
                return TRUE;
            }
            else if (wId == IDCANCEL || wId == IDOK)
            {
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;
        }

        case WM_CLOSE:
        {
            EndDialog(hwndDlg, 0);
            return TRUE;
        }
    }

    return FALSE;
}

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&iccx);

    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_ERRORTEST), NULL, ErrorTestDlgProc, 0);
    return 0;
}
