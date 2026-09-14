/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS GUI/console setup
 * FILE:            base/setup/setup/setup.c
 * PURPOSE:         Second stage setup
 * PROGRAMMER:      Eric Kohl
 */

#include <stdarg.h>
#define WIN32_NO_STATUS
#define _INC_WINDOWS
#define COM_NO_WINDOWS_H
#include <windef.h>
#include <winbase.h>

#define NTOS_MODE_USER
#include <ndk/setypes.h>
#include <ndk/rtlfuncs.h>
#include <ndk/exfuncs.h>

#define NDEBUG
#include <debug.h>

typedef INT (WINAPI *PINSTALL_REACTOS)(INT argc, WCHAR** argv);

/* FUNCTIONS ****************************************************************/

static
INT
RunInstallReactOS(INT argc, WCHAR* argv[])
{
    INT RetVal;
    HMODULE hDll;
    PINSTALL_REACTOS InstallReactOS;

    hDll = LoadLibraryW(L"syssetup.dll");
    if (hDll == NULL)
    {
        DPRINT("Failed to load 'syssetup.dll'!\n");
        return GetLastError();
    }
    DPRINT("Loaded 'syssetup.dll'!\n");

    /* Call the standard Windows-compatible export */
    InstallReactOS = (PINSTALL_REACTOS)GetProcAddress(hDll, "InstallWindowsNt");
    if (InstallReactOS == NULL)
    {
        RetVal = GetLastError();
        DPRINT("Failed to get address for 'InstallWindowsNt()'!\n");
    }
    else
    {
        RetVal = InstallReactOS(argc, argv);
    }

    return RetVal;
}


/* Called from wmainCRTStartup */
INT wmain(INT argc, WCHAR* argv[])
{
    LPWSTR CmdLine, p;

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    // NOTE: Temporary, until we correctly use argc/argv.
    CmdLine = GetCommandLineW();
    DPRINT("CmdLine: <%S>\n", CmdLine);

    p = wcschr(CmdLine, L'-');
    if (p == NULL)
        return ERROR_INVALID_PARAMETER;
    p++;

    // NOTE: On Windows, "mini" means "minimal UI", and can be used
    // in addition to "newsetup"; these options are not exclusive.
    if (_wcsicmp(p, L"newsetup") == 0)
    {
        RunInstallReactOS(argc, argv);

        /* Force reboot ONLY for newsetup (HDD installation stage 2) */
        BOOLEAN Old = FALSE;
        RtlAdjustPrivilege(SE_SHUTDOWN_PRIVILEGE, TRUE, FALSE, &Old);
        NtShutdownSystem(ShutdownReboot);
        TerminateProcess(GetCurrentProcess(), 0);
        return 0;
    }
    else if (_wcsicmp(p, L"mini") == 0)
    {
        RunInstallReactOS(argc, argv);
        /* In LiveCD (mini) mode, do NOT reboot! Just exit setup and stay in live session */
        return 0;
    }

    return 0;
}

/* EOF */
