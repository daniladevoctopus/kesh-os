/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS Winlogon
 * FILE:            base/system/winlogon/setup.c
 * PURPOSE:         Setup support functions
 * PROGRAMMERS:     Eric Kohl
 */

/* INCLUDES *****************************************************************/

#include "winlogon.h"
#include <ndk/setypes.h>

/* FUNCTIONS ****************************************************************/

DWORD
GetSetupType(VOID)
{
    DWORD dwError;
    HKEY hKey;
    DWORD dwType;
    DWORD dwSize;
    DWORD dwSetupType;

    TRACE("GetSetupType()\n");

    /* Open key */
    dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"SYSTEM\\Setup",
                            0,
                            KEY_QUERY_VALUE,
                            &hKey);
    if (dwError != ERROR_SUCCESS)
        return 0;

    /* Read key */
    dwSize = sizeof(DWORD);
    dwError = RegQueryValueExW(hKey,
                               L"SetupType",
                               NULL,
                               &dwType,
                               (LPBYTE)&dwSetupType,
                               &dwSize);

    /* Close key, and check if returned values are correct */
    RegCloseKey(hKey);
    if (dwError != ERROR_SUCCESS || dwType != REG_DWORD || dwSize != sizeof(DWORD))
        return 0;

    TRACE("GetSetupType() returns %lu\n", dwSetupType);
    return dwSetupType;
}


static
DWORD
WINAPI
RunSetupThreadProc(
    IN LPVOID lpParameter)
{
    PROCESS_INFORMATION ProcessInformation;
    STARTUPINFOW StartupInfo;
    WCHAR Shell[MAX_PATH];
    WCHAR CommandLine[MAX_PATH];
    BOOL Result;
    DWORD dwError;
    HKEY hKey;
    DWORD dwType;
    DWORD dwSize;
    DWORD dwExitCode;

    TRACE("RunSetup() called\n");

    /* Open key */
    dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"SYSTEM\\Setup",
                            0,
                            KEY_QUERY_VALUE,
                            &hKey);
    if (dwError != ERROR_SUCCESS)
        return FALSE;

    /* Read key */
    dwSize = sizeof(Shell);
    dwError = RegQueryValueExW(hKey,
                               L"CmdLine",
                               NULL,
                               &dwType,
                               (LPBYTE)Shell,
                               &dwSize);
    RegCloseKey(hKey);
    if (dwError != ERROR_SUCCESS)
        return FALSE;

    /* Finish string */
    Shell[dwSize / sizeof(WCHAR)] = UNICODE_NULL;

    /* Expand string (if applicable) */
    if (dwType == REG_EXPAND_SZ)
        ExpandEnvironmentStringsW(Shell, CommandLine, ARRAYSIZE(CommandLine));
    else if (dwType == REG_SZ)
        wcscpy(CommandLine, Shell);
    else
        return FALSE;

    TRACE("Should run '%s' now\n", debugstr_w(CommandLine));

    SwitchDesktop(WLSession->ApplicationDesktop);

    /* Start process */
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.lpReserved = NULL;
    StartupInfo.lpDesktop = L"WinSta0\\Default";
    StartupInfo.lpTitle = NULL;
    StartupInfo.dwFlags = 0;
    StartupInfo.cbReserved2 = 0;
    StartupInfo.lpReserved2 = 0;

    Result = CreateProcessW(NULL,
                            CommandLine,
                            NULL,
                            NULL,
                            FALSE,
                            DETACHED_PROCESS,
                            NULL,
                            NULL,
                            &StartupInfo,
                            &ProcessInformation);
    if (!Result)
    {
        TRACE("Failed to run setup process\n");
        SwitchDesktop(WLSession->WinlogonDesktop);
        return FALSE;
    }

    /* Wait for process termination */
    WaitForSingleObject(ProcessInformation.hProcess, INFINITE);

    GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode);

    /* Close handles */
    CloseHandle(ProcessInformation.hThread);
    CloseHandle(ProcessInformation.hProcess);

    TRACE ("RunSetup() done\n");

    /* If this is LiveCD mode ("mini"), do NOT reboot! Stay in the live session */
    if (wcsstr(CommandLine, L"mini") != NULL)
    {
        TRACE("LiveCD setup completed, staying in session\n");
        return TRUE;
    }

    /* Mark setup complete in registry so setup never runs again in a loop */
    {
        HKEY hSetupKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\Setup", 0, KEY_SET_VALUE, &hSetupKey) == ERROR_SUCCESS)
        {
            DWORD zero = 0;
            WCHAR empty[] = L"";
            RegSetValueExW(hSetupKey, L"SetupType", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegSetValueExW(hSetupKey, L"SystemSetupInProgress", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegSetValueExW(hSetupKey, L"CmdLine", 0, REG_SZ, (const BYTE*)empty, sizeof(empty));
            RegFlushKey(hSetupKey);
            RegCloseKey(hSetupKey);
        }
    }

    /* Setup has finished - immediately reboot the machine into the installed system */
    {
        BOOLEAN Old = FALSE;
        RtlAdjustPrivilege(SE_SHUTDOWN_PRIVILEGE, TRUE, FALSE, &Old);
        NtShutdownSystem(ShutdownReboot);
    }

    /* If direct kernel reboot didn't restart, trigger full winlogon shutdown */
    HandleShutdown(WLSession, WLX_SAS_ACTION_SHUTDOWN_REBOOT);

    return TRUE;
}


BOOL
RunSetup(VOID)
{
    HANDLE hThread;

    hThread = CreateThread(NULL,
                           0,
                           RunSetupThreadProc,
                           NULL,
                           0,
                           NULL);
    if (hThread != NULL)
        CloseHandle(hThread);

    return hThread != NULL;
}

/* EOF */
