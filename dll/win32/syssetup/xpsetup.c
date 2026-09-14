/*
 * PROJECT:     KeshOS NT 0.8.0 Beta "Brownie" System Setup
 * LICENSE:     GPL-2.0-or-later
 * PURPOSE:     Windows XP Styled Stage 2 GUI Setup Screen
 * DEVELOPERS:  SneakDeak Team & KeshOS Project
 */

#include "precomp.h"
#include <time.h>
#include <commctrl.h>
#define NDEBUG
#include <debug.h>

#define IDT_ANIMATE 101
#define IDT_SLIDE   102

#define WM_XPSETUP_UPDATE   (WM_USER + 100)
#define WM_XPSETUP_FINISHED (WM_USER + 101)

typedef struct _XPSETUP_STATE {
    HWND hWnd;
    int CurrentProgress;    /* 0 - 100 */
    int CurrentSlide;       /* 0 - 4 */
    int AnimTick;
    int MinutesRemaining;
    WCHAR szStatus[256];
    CRITICAL_SECTION csLock;
    BOOL bFinished;
} XPSETUP_STATE;

static XPSETUP_STATE g_XPState;

/* ── Colors: Rich Brownie Chocolate Palette ── */
#define CLR_HDR_TOP       RGB(38, 22, 16)    /* Dark Espresso */
#define CLR_HDR_BOT       RGB(68, 42, 32)    /* Rich Chocolate */
#define CLR_HDR_BRD       RGB(180, 130, 75)  /* Caramel Gold */
#define CLR_SIDEBAR_BG    RGB(46, 28, 22)    /* Deep Brownie */
#define CLR_SIDEBAR_BRD   RGB(85, 54, 42)    /* Chocolate Border */
#define CLR_MAIN_BG       RGB(252, 248, 242) /* Creamy Vanilla Background */
#define CLR_BOTTOM_BG     RGB(30, 16, 12)    /* Dark Chocolate */
#define CLR_BOTTOM_BRD    RGB(95, 62, 46)    /* Caramel Line */
#define CLR_PROG_BG       RGB(45, 26, 20)    /* Progress Track */
#define CLR_PROG_BRD      RGB(125, 82, 55)   /* Progress Border */
#define CLR_PROG_FILL     RGB(245, 185, 75)  /* Golden Amber Fill Top */
#define CLR_PROG_FILL2    RGB(205, 130, 45)  /* Warm Caramel Fill Bottom */
#define CLR_TEXT_WHITE    RGB(255, 255, 255)
#define CLR_TEXT_GOLD     RGB(245, 212, 140) /* Warm Honey Gold */
#define CLR_TEXT_AMBER    RGB(240, 175, 75)  /* Vivid Amber */
#define CLR_TEXT_MUTED    RGB(195, 170, 155) /* Warm Grey-Brown */
#define CLR_TEXT_DARK     RGB(60, 32, 20)    /* Rich Cocoa */
#define CLR_TEXT_BODY     RGB(75, 55, 45)    /* Warm Coffee Text */

/* ── Billboards ── */
typedef struct _BILLBOARD_SLIDE {
    LPCWSTR Title;
    LPCWSTR Subtitle;
    LPCWSTR Points[3];
} BILLBOARD_SLIDE;

static const BILLBOARD_SLIDE g_Slides[] = {
    {
        L"Welcome to KeshOS NT Beta \"Brownie\"",
        L"A modern, responsive, and lightweight operating system built on classic NT architecture.",
        {
            L"Ultra-fast response time and exceptionally low memory consumption.",
            L"Fully automated installation and essential hardware configuration.",
            L"Original KeshOS innovations on top of a reliable system core."
        }
    },
    {
        L"Exclusive \"Brownie\" Visual Style",
        L"Comfortable warm chocolate-coffee palette designed for everyday productivity.",
        {
            L"Harmonious warm tones designed to reduce visual fatigue during long sessions.",
            L"Crisp typography, styled dialogs, and cohesive interface elements.",
            L"Seamless toggle between Brownie visual style and classic NT appearance."
        }
    },
    {
        L"High Software Compatibility",
        L"Run your essential tools, productivity suites, and classic games without hurdles.",
        {
            L"Reliable support for classic 32-bit Windows NT, 2000, and XP software.",
            L"Stable performance for media players, archive utilities, and office suites.",
            L"Zero telemetry, no forced bloatware, and no background slowdowns."
        }
    },
    {
        L"KeshOS Application Manager",
        L"Hundreds of essential utilities and verified packages available in one click.",
        {
            L"Quickly discover web browsers, developer tools, and system diagnostics.",
            L"Convenient software installation and package updates directly from catalog.",
            L"Straightforward integration with community software repositories."
        }
    },
    {
        L"Out-of-Box Experience (OOBE)",
        L"Personalize your system preferences right after the installation restarts.",
        {
            L"Select your preferred language from 12 real-time locales.",
            L"Create your personal user account with custom password protection.",
            L"Your system will be ready for immediate and secure everyday work."
        }
    }
};

#define SLIDE_COUNT (sizeof(g_Slides) / sizeof(g_Slides[0]))

/* ── UI Helpers ── */

static void DrawGradient(HDC hdc, RECT* rc, COLORREF cTop, COLORREF cBot)
{
    TRIVERTEX vert[2];
    GRADIENT_RECT gRect;

    vert[0].x     = rc->left;
    vert[0].y     = rc->top;
    vert[0].Red   = (COLOR16)(GetRValue(cTop) << 8);
    vert[0].Green = (COLOR16)(GetGValue(cTop) << 8);
    vert[0].Blue  = (COLOR16)(GetBValue(cTop) << 8);
    vert[0].Alpha = 0x0000;

    vert[1].x     = rc->right;
    vert[1].y     = rc->bottom;
    vert[1].Red   = (COLOR16)(GetRValue(cBot) << 8);
    vert[1].Green = (COLOR16)(GetGValue(cBot) << 8);
    vert[1].Blue  = (COLOR16)(GetBValue(cBot) << 8);
    vert[1].Alpha = 0x0000;

    gRect.UpperLeft  = 0;
    gRect.LowerRight = 1;

    GdiGradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

static void DrawRealLogo(HDC hdc, int x, int y)
{
    HBITMAP hBmp = LoadBitmapW(hDllInstance, MAKEINTRESOURCEW(IDB_KESHOS_HDR));
    if (hBmp)
    {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);
        BitBlt(hdc, x, y, 36, 36, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        DeleteObject(hBmp);
    }
}

static void DrawCheckMark(HDC hdc, int x, int y, COLORREF color)
{
    HPEN hPen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, x, y + 6, NULL);
    LineTo(hdc, x + 4, y + 11);
    LineTo(hdc, x + 11, y + 2);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

static void DrawArrowMark(HDC hdc, int x, int y, COLORREF color)
{
    POINT pts[3] = { { x, y + 2 }, { x + 8, y + 7 }, { x, y + 12 } };
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, hNoPen);
    Polygon(hdc, pts, 3);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hNoPen);
}

static void UpdateStatus(LPCWSTR szText, int nProgress, int nMinutesRemaining)
{
    EnterCriticalSection(&g_XPState.csLock);
    if (szText)
    {
        wcsncpy(g_XPState.szStatus, szText, 255);
        g_XPState.szStatus[255] = L'\0';
    }
    if (nProgress >= 0)
    {
        if (nProgress > 100) nProgress = 100;
        g_XPState.CurrentProgress = nProgress;
    }
    if (nMinutesRemaining >= 0)
    {
        g_XPState.MinutesRemaining = nMinutesRemaining;
    }
    LeaveCriticalSection(&g_XPState.csLock);

    if (g_XPState.hWnd)
    {
        PostMessageW(g_XPState.hWnd, WM_XPSETUP_UPDATE, 0, 0);
    }
}

/* ── Worker Registration Notification ── */

typedef struct _XPREG_NOTIFY {
    ULONG DllCount;
    ULONG Registered;
} XPREG_NOTIFY;

static UINT CALLBACK XPRegistrationNotificationProc(
    PVOID Context,
    UINT Notification,
    UINT_PTR Param1,
    UINT_PTR Param2)
{
    XPREG_NOTIFY* pReg = (XPREG_NOTIFY*)Context;

    if (Notification == SPFILENOTIFY_STARTREGISTRATION)
    {
        PSP_REGISTER_CONTROL_STATUSW StatusInfo = (PSP_REGISTER_CONTROL_STATUSW)Param1;
        if (StatusInfo && StatusInfo->FileName)
        {
            PCWSTR pszFile = wcsrchr(StatusInfo->FileName, L'\\');
            if (pszFile) pszFile++;
            else pszFile = StatusInfo->FileName;

            WCHAR szBuf[256];
            _snwprintf(szBuf, ARRAYSIZE(szBuf), L"Регистрация компонентов: %s", pszFile);

            int nProg = 10;
            if (pReg && pReg->DllCount > 0)
            {
                nProg = 10 + (int)((pReg->Registered * 60) / pReg->DllCount);
            }
            int nMin = 9 - (nProg / 15);
            if (nMin < 2) nMin = 2;

            UpdateStatus(szBuf, nProg, nMin);
        }
        return FILEOP_DOIT;
    }
    else if (Notification == SPFILENOTIFY_ENDREGISTRATION)
    {
        if (pReg)
            pReg->Registered++;
        return FILEOP_DOIT;
    }

    return FILEOP_DOIT;
}

/* ── Worker Thread ── */

static DWORD WINAPI XPSetupWorkerThread(LPVOID lpParam)
{
    HWND hWnd = (HWND)lpParam;
    Sleep(1200);

    UpdateStatus(L"Preparing KeshOS system environment...", 5, 10);
    Sleep(800);

    /* 1. Register DLL components from RegistrationPhase2 */
    UpdateStatus(L"Registering system components and libraries...", 10, 9);
    {
        INFCONTEXT Context;
        WCHAR SectionName[512] = L"";
        LONG DllCount = 0;

        if (SetupFindFirstLineW(hSysSetupInf, L"RegistrationPhase2", L"RegisterDlls", &Context) &&
            SetupGetStringFieldW(&Context, 1, SectionName, ARRAYSIZE(SectionName), NULL))
        {
            DllCount = SetupGetLineCountW(hSysSetupInf, SectionName);
        }
        if (DllCount <= 0) DllCount = 100;

        XPREG_NOTIFY regNotify;
        regNotify.DllCount = (ULONG)DllCount;
        regNotify.Registered = 0;

        _SEH2_TRY
        {
            SetupInstallFromInfSectionW(hWnd,
                                         hSysSetupInf,
                                         L"RegistrationPhase2",
                                         SPINST_REGISTRY | SPINST_REGISTERCALLBACKAWARE | SPINST_REGSVR,
                                         0, NULL, 0,
                                         XPRegistrationNotificationProc,
                                         &regNotify,
                                         NULL, NULL);
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            DPRINT1("Exception during SetupInstallFromInfSectionW\n");
        }
        _SEH2_END;
    }

    /* 2. Register TypeLibraries */
    UpdateStatus(L"Registering COM TypeLibraries...", 72, 4);
    _SEH2_TRY
    {
        RegisterTypeLibraries(NULL, NULL, hSysSetupInf, L"TypeLibraries");
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        DPRINT1("Exception during RegisterTypeLibraries\n");
    }
    _SEH2_END;
    Sleep(400);

    /* 3. Install Start Menu items */
    UpdateStatus(L"Creating Start Menu items and shortcuts...", 80, 3);
    _SEH2_TRY
    {
        ITEMSDATA items;
        ZeroMemory(&items, sizeof(items));
        items.hwndDlg = hWnd;
        InstallStartMenuItems(&items);
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        DPRINT1("Exception during InstallStartMenuItems\n");
    }
    _SEH2_END;
    Sleep(400);

    /* 4. Configure Computer Name and Workgroup */
    UpdateStatus(L"Configuring computer name and workgroup...", 86, 2);
    _SEH2_TRY
    {
        WCHAR szCompName[MAX_COMPUTERNAME_LENGTH + 1] = L"KESHOS-PC";
        SetComputerNameW(szCompName);
        SetComputerNameExW(ComputerNamePhysicalDnsHostname, szCompName);
        SetComputerNameExW(ComputerNamePhysicalDnsDomain, L"");

        HKEY hKey;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                            0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, L"Hostname", 0, REG_SZ, (const BYTE*)szCompName, (lstrlenW(szCompName) + 1) * sizeof(WCHAR));
            RegSetValueExW(hKey, L"NV Hostname", 0, REG_SZ, (const BYTE*)szCompName, (lstrlenW(szCompName) + 1) * sizeof(WCHAR));
            RegCloseKey(hKey);
        }

        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\LanmanWorkstation\\Parameters",
                            0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, L"Domain", 0, REG_SZ, (const BYTE*)L"WORKGROUP", sizeof(L"WORKGROUP"));
            RegCloseKey(hKey);
        }

        SetAdministratorPassword(L"");
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        DPRINT1("Exception during network config\n");
    }
    _SEH2_END;
    Sleep(400);

    /* 5. Save Security and Settings */
    UpdateStatus(L"Saving security settings and system configuration...", 92, 1);
    {
        ITEMSDATA items;
        ZeroMemory(&items, sizeof(items));
        items.hwndDlg = hWnd;

        REGISTRATIONNOTIFY notify;
        ZeroMemory(&notify, sizeof(notify));

        _SEH2_TRY
        {
            InstallSecurity(&items, &notify);
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            DPRINT1("Exception during InstallSecurity\n");
        }
        _SEH2_END;

        _SEH2_TRY
        {
            RtlCreateBootStatusDataFile();
        }
        _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
        {
            DPRINT1("Exception during RtlCreateBootStatusDataFile\n");
        }
        _SEH2_END;
    }
    Sleep(500);

    /* 6. Finalize installation in registry */
    UpdateStatus(L"Completing installation and finalizing settings...", 98, 1);
    {
        HKEY hKey = 0;
        DWORD dwInstallDate = (DWORD)time(NULL);

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion",
                          0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegSetValueExW(hKey, L"InstallDate", 0, REG_DWORD, (const BYTE*)&dwInstallDate, sizeof(dwInstallDate));
            RegCloseKey(hKey);
        }

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\Setup", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
        {
            DWORD zero = 0;
            WCHAR empty[] = L"";
            RegSetValueExW(hKey, L"SetupType", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegSetValueExW(hKey, L"SystemSetupInProgress", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegSetValueExW(hKey, L"CmdLine", 0, REG_SZ, (const BYTE*)empty, sizeof(empty));
            RegFlushKey(hKey);
            RegCloseKey(hKey);
        }
    }

    UpdateStatus(L"Setup completed successfully! Preparing to restart...", 100, 0);
    Sleep(2000);

    EnterCriticalSection(&g_XPState.csLock);
    g_XPState.bFinished = TRUE;
    LeaveCriticalSection(&g_XPState.csLock);

    PostMessageW(hWnd, WM_XPSETUP_FINISHED, 0, 0);
    return 0;
}

/* ── Window Procedure ── */

static LRESULT CALLBACK XPSetupWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            g_XPState.hWnd = hWnd;
            SetTimer(hWnd, IDT_ANIMATE, 200, NULL);
            SetTimer(hWnd, IDT_SLIDE, 7000, NULL);

            HANDLE hThread = CreateThread(NULL, 0, XPSetupWorkerThread, (LPVOID)hWnd, 0, NULL);
            if (hThread)
                CloseHandle(hThread);
            break;
        }

        case WM_TIMER:
        {
            if (wParam == IDT_ANIMATE)
            {
                g_XPState.AnimTick++;
                /* Invalidate progress bar and status area */
                RECT rc;
                GetClientRect(hWnd, &rc);
                RECT rcBottom = { 0, rc.bottom - 80, rc.right, rc.bottom };
                InvalidateRect(hWnd, &rcBottom, FALSE);

                RECT rcSidebarAnim = { 15, rc.bottom - 170, 245, rc.bottom - 90 };
                InvalidateRect(hWnd, &rcSidebarAnim, FALSE);
            }
            else if (wParam == IDT_SLIDE)
            {
                g_XPState.CurrentSlide = (g_XPState.CurrentSlide + 1) % SLIDE_COUNT;
                RECT rc;
                GetClientRect(hWnd, &rc);
                RECT rcBillboard = { 260, 55, rc.right, rc.bottom - 80 };
                InvalidateRect(hWnd, &rcBillboard, TRUE);
            }
            break;
        }

        case WM_XPSETUP_UPDATE:
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            RECT rcBottom = { 0, rc.bottom - 80, rc.right, rc.bottom };
            InvalidateRect(hWnd, &rcBottom, FALSE);
            break;
        }

        case WM_XPSETUP_FINISHED:
        {
            KillTimer(hWnd, IDT_ANIMATE);
            KillTimer(hWnd, IDT_SLIDE);
            DestroyWindow(hWnd);
            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);
            int width = rc.right;
            int height = rc.bottom;

            /* Double buffering to eliminate flicker */
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            /* Fonts */
            HFONT hFontHeader = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontStepHdr = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontStep = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontStepBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontSlideTitle = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontSlideSub = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontSlideText = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
            HFONT hFontStatus = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");

            /* 1. Header Bar (Height: 52px) */
            RECT rcHeader = { 0, 0, width, 52 };
            DrawGradient(memDC, &rcHeader, CLR_HDR_TOP, CLR_HDR_BOT);

            /* Real KeshOS Logo from resource */
            DrawRealLogo(memDC, 16, 8);

            SelectObject(memDC, hFontHeader);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, CLR_TEXT_WHITE);
            TextOutW(memDC, 62, 15, L"KeshOS NT 0.8.0 Beta «Brownie» — Setup", 38);

            /* Brownie Edition Badge in top right of header */
            RECT rcPill = { width - 115, 14, width - 20, 38 };
            HBRUSH hPillBrush = CreateSolidBrush(RGB(190, 125, 55));
            HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
            HBRUSH oldPillB = (HBRUSH)SelectObject(memDC, hPillBrush);
            HPEN oldPillP = (HPEN)SelectObject(memDC, hNoPen);
            RoundRect(memDC, rcPill.left, rcPill.top, rcPill.right, rcPill.bottom, 10, 10);
            SelectObject(memDC, oldPillB);
            SelectObject(memDC, oldPillP);
            DeleteObject(hPillBrush);
            DeleteObject(hNoPen);

            SetTextColor(memDC, RGB(255, 255, 255));
            SelectObject(memDC, hFontStepBold);
            DrawTextW(memDC, L"BROWNIE", 7, &rcPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            /* Header border line */
            HPEN hPenLine = CreatePen(PS_SOLID, 1, CLR_HDR_BRD);
            HPEN oldPen = (HPEN)SelectObject(memDC, hPenLine);
            MoveToEx(memDC, 0, 52, NULL);
            LineTo(memDC, width, 52);

            /* 2. Left Sidebar (Width: 250px) */
            RECT rcSidebar = { 0, 53, 250, height - 80 };
            HBRUSH hBrushSide = CreateSolidBrush(CLR_SIDEBAR_BG);
            FillRect(memDC, &rcSidebar, hBrushSide);
            DeleteObject(hBrushSide);

            /* Right border of sidebar */
            HPEN hPenSide = CreatePen(PS_SOLID, 1, CLR_SIDEBAR_BRD);
            SelectObject(memDC, hPenSide);
            MoveToEx(memDC, 250, 53, NULL);
            LineTo(memDC, 250, height - 80);
            DeleteObject(hPenSide);

            /* Sidebar Content */
            SelectObject(memDC, hFontStepHdr);
            SetTextColor(memDC, CLR_TEXT_GOLD);
            TextOutW(memDC, 20, 75, L"Installation Steps", 18);

            int stepY = 110;

            /* Step 1 - Completed */
            DrawCheckMark(memDC, 22, stepY, CLR_TEXT_AMBER);
            SetTextColor(memDC, CLR_TEXT_MUTED);
            SelectObject(memDC, hFontStep);
            TextOutW(memDC, 44, stepY, L"Collecting information", 22);
            stepY += 28;

            /* Step 2 - Completed */
            DrawCheckMark(memDC, 22, stepY, CLR_TEXT_AMBER);
            SetTextColor(memDC, CLR_TEXT_MUTED);
            SelectObject(memDC, hFontStep);
            TextOutW(memDC, 44, stepY, L"Dynamic update", 14);
            stepY += 28;

            /* Step 3 - Completed */
            DrawCheckMark(memDC, 22, stepY, CLR_TEXT_AMBER);
            SetTextColor(memDC, CLR_TEXT_MUTED);
            SelectObject(memDC, hFontStep);
            TextOutW(memDC, 44, stepY, L"Preparing installation", 22);
            stepY += 32;

            /* Step 4 - ACTIVE */
            DrawArrowMark(memDC, 22, stepY, CLR_TEXT_GOLD);
            SetTextColor(memDC, CLR_TEXT_WHITE);
            SelectObject(memDC, hFontStepBold);
            TextOutW(memDC, 44, stepY, L"Installing KeshOS", 17);
            stepY += 24;

            /* Sub-steps of Step 4 */
            SetTextColor(memDC, CLR_TEXT_GOLD);
            SelectObject(memDC, hFontStep);
            TextOutW(memDC, 52, stepY, L"• Registering components", 24);
            stepY += 22;
            TextOutW(memDC, 52, stepY, L"• Network and services", 22);
            stepY += 22;
            TextOutW(memDC, 52, stepY, L"• Saving configuration", 22);
            stepY += 32;

            /* Step 5 - Upcoming */
            HPEN hDotPen = CreatePen(PS_SOLID, 2, RGB(140, 115, 100));
            HPEN oldDotP = (HPEN)SelectObject(memDC, hDotPen);
            Ellipse(memDC, 24, stepY + 3, 32, stepY + 11);
            SelectObject(memDC, oldDotP);
            DeleteObject(hDotPen);

            SetTextColor(memDC, RGB(140, 115, 100));
            SelectObject(memDC, hFontStep);
            TextOutW(memDC, 44, stepY, L"Completing installation", 23);

            /* Sidebar bottom - Remaining Time Box */
            int timeBoxY = height - 175;
            RECT rcTimeBox = { 16, timeBoxY, 234, timeBoxY + 75 };
            HBRUSH hBrushBox = CreateSolidBrush(RGB(32, 18, 14));
            HPEN hPenBox = CreatePen(PS_SOLID, 1, RGB(115, 78, 55));
            SelectObject(memDC, hBrushBox);
            SelectObject(memDC, hPenBox);
            RoundRect(memDC, rcTimeBox.left, rcTimeBox.top, rcTimeBox.right, rcTimeBox.bottom, 12, 12);
            DeleteObject(hBrushBox);
            DeleteObject(hPenBox);

            SetTextColor(memDC, CLR_TEXT_MUTED);
            SelectObject(memDC, hFontStep);
            TextOutW(memDC, 28, timeBoxY + 12, L"Estimated time:", 15);

            EnterCriticalSection(&g_XPState.csLock);
            int minRem = g_XPState.MinutesRemaining;
            LeaveCriticalSection(&g_XPState.csLock);

            WCHAR szMin[64];
            if (minRem > 0)
                _snwprintf(szMin, ARRAYSIZE(szMin), L"Remaining: ~%d min.", minRem);
            else
                _snwprintf(szMin, ARRAYSIZE(szMin), L"Finalizing setup...");

            SetTextColor(memDC, CLR_TEXT_GOLD);
            SelectObject(memDC, hFontStepBold);
            TextOutW(memDC, 28, timeBoxY + 34, szMin, lstrlenW(szMin));

            /* Animated activity dots */
            int dotX = 175;
            int dotY = timeBoxY + 42;
            int activeDot = (g_XPState.AnimTick / 2) % 4;
            for (int d = 0; d < 4; d++)
            {
                COLORREF cDot = (d == activeDot) ? CLR_TEXT_AMBER : RGB(75, 45, 32);
                HBRUSH hDotBrush = CreateSolidBrush(cDot);
                SelectObject(memDC, hDotBrush);
                HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
                SelectObject(memDC, hNoPen);
                Ellipse(memDC, dotX + d * 12, dotY, dotX + d * 12 + 6, dotY + 6);
                DeleteObject(hDotBrush);
                DeleteObject(hNoPen);
            }

            /* 3. Main Billboard Area (X: 251 to width, Y: 53 to height - 80) */
            RECT rcMain = { 251, 53, width, height - 80 };
            HBRUSH hBrushMain = CreateSolidBrush(CLR_MAIN_BG);
            FillRect(memDC, &rcMain, hBrushMain);
            DeleteObject(hBrushMain);

            /* Current Slide */
            int slideIdx = g_XPState.CurrentSlide;
            if (slideIdx < 0 || slideIdx >= (int)SLIDE_COUNT)
                slideIdx = 0;
            const BILLBOARD_SLIDE* pSlide = &g_Slides[slideIdx];

            int bbX = 285;
            int bbY = 85;

            SelectObject(memDC, hFontSlideTitle);
            SetTextColor(memDC, CLR_TEXT_DARK);
            TextOutW(memDC, bbX, bbY, pSlide->Title, lstrlenW(pSlide->Title));
            bbY += 40;

            SelectObject(memDC, hFontSlideSub);
            SetTextColor(memDC, CLR_TEXT_BODY);
            RECT rcSub = { bbX, bbY, width - 40, bbY + 55 };
            DrawTextW(memDC, pSlide->Subtitle, -1, &rcSub, DT_WORDBREAK);
            bbY += 60;

            /* Decorative line under subtitle */
            HPEN hPenDeco = CreatePen(PS_SOLID, 2, RGB(215, 180, 145));
            SelectObject(memDC, hPenDeco);
            MoveToEx(memDC, bbX, bbY, NULL);
            LineTo(memDC, min(bbX + 480, width - 40), bbY);
            DeleteObject(hPenDeco);
            bbY += 32;

            /* Slide Bullet Points */
            SelectObject(memDC, hFontSlideText);
            SetTextColor(memDC, CLR_TEXT_BODY);
            for (int p = 0; p < 3; p++)
            {
                if (pSlide->Points[p] && pSlide->Points[p][0])
                {
                    /* Bullet icon: Warm Amber Diamond */
                    HBRUSH hBul = CreateSolidBrush(CLR_TEXT_AMBER);
                    SelectObject(memDC, hBul);
                    HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
                    SelectObject(memDC, hNoPen);
                    Ellipse(memDC, bbX, bbY + 6, bbX + 9, bbY + 15);
                    DeleteObject(hBul);
                    DeleteObject(hNoPen);

                    RECT rcPt = { bbX + 22, bbY, width - 40, bbY + 40 };
                    DrawTextW(memDC, pSlide->Points[p], -1, &rcPt, DT_WORDBREAK);
                    bbY += 40;
                }
            }

            /* 4. Bottom Status Panel (Height: 80px) */
            RECT rcBottom = { 0, height - 80, width, height };
            DrawGradient(memDC, &rcBottom, CLR_BOTTOM_BG, RGB(20, 10, 8));

            /* Top border of bottom panel */
            HPEN hPenBot = CreatePen(PS_SOLID, 1, CLR_BOTTOM_BRD);
            SelectObject(memDC, hPenBot);
            MoveToEx(memDC, 0, height - 80, NULL);
            LineTo(memDC, width, height - 80);
            DeleteObject(hPenBot);

            /* Status Text */
            EnterCriticalSection(&g_XPState.csLock);
            WCHAR szCurStatus[256];
            wcscpy(szCurStatus, g_XPState.szStatus);
            int curProg = g_XPState.CurrentProgress;
            LeaveCriticalSection(&g_XPState.csLock);

            SelectObject(memDC, hFontStatus);
            SetTextColor(memDC, CLR_TEXT_WHITE);
            TextOutW(memDC, 35, height - 68, szCurStatus, lstrlenW(szCurStatus));

            /* Progress Bar */
            int pbX = 35;
            int pbY = height - 38;
            int pbW = width - 70;
            int pbH = 18;

            /* Progress Bar Background */
            HBRUSH hProgBg = CreateSolidBrush(CLR_PROG_BG);
            HPEN hProgBrd = CreatePen(PS_SOLID, 1, CLR_PROG_BRD);
            SelectObject(memDC, hProgBg);
            SelectObject(memDC, hProgBrd);
            RoundRect(memDC, pbX, pbY, pbX + pbW, pbY + pbH, 8, 8);
            DeleteObject(hProgBg);
            DeleteObject(hProgBrd);

            /* Progress Bar Fill */
            int fillW = (pbW * curProg) / 100;
            if (fillW > 0)
            {
                RECT rcFill = { pbX + 1, pbY + 1, pbX + fillW, pbY + pbH - 1 };
                DrawGradient(memDC, &rcFill, CLR_PROG_FILL, CLR_PROG_FILL2);
            }

            /* Clean up GDI objects */
            SelectObject(memDC, oldPen);
            DeleteObject(hPenLine);

            DeleteObject(hFontHeader);
            DeleteObject(hFontStepHdr);
            DeleteObject(hFontStep);
            DeleteObject(hFontStepBold);
            DeleteObject(hFontSlideTitle);
            DeleteObject(hFontSlideSub);
            DeleteObject(hFontSlideText);
            DeleteObject(hFontStatus);

            /* BitBlt to screen */
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

/* ── Main Entrypoint for Stage 2 XP Setup ── */

VOID RunXPSetup(VOID)
{
    HINSTANCE hInstance = hDllInstance;

    ZeroMemory(&g_XPState, sizeof(g_XPState));
    InitializeCriticalSection(&g_XPState.csLock);
    g_XPState.CurrentProgress = 0;
    g_XPState.CurrentSlide = 0;
    g_XPState.MinutesRemaining = 10;
    wcscpy(g_XPState.szStatus, L"Initializing KeshOS Setup...");

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = XPSetupWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"KeshOS_XPSetup_Class";

    RegisterClassExW(&wc);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);

    HWND hWnd = CreateWindowExW(WS_EX_TOPMOST,
                                L"KeshOS_XPSetup_Class",
                                L"KeshOS Setup",
                                WS_POPUP | WS_VISIBLE,
                                0, 0, scrW, scrH,
                                NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        DPRINT1("CreateWindowExW failed for XPSetup!\n");
        DeleteCriticalSection(&g_XPState.csLock);
        return;
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(L"KeshOS_XPSetup_Class", hInstance);
    DeleteCriticalSection(&g_XPState.csLock);
}
