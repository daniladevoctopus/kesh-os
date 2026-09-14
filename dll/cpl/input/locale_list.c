/*
 * PROJECT:         input.dll
 * FILE:            dll/cpl/input/locale_list.c
 * PURPOSE:         input.dll
 * PROGRAMMER:      Dmitry Chapyshev (dmitry@reactos.org)
 */

#include "locale_list.h"


static LOCALE_LIST_NODE *_LocaleList = NULL;


static LOCALE_LIST_NODE*
LocaleList_Append(DWORD dwId, const WCHAR *pszName)
{
    LOCALE_LIST_NODE *pCurrent;
    LOCALE_LIST_NODE *pNew;

    if (pszName == NULL)
        return NULL;

    pCurrent = _LocaleList;

    pNew = (LOCALE_LIST_NODE*)malloc(sizeof(LOCALE_LIST_NODE));
    if (pNew == NULL)
        return NULL;

    ZeroMemory(pNew, sizeof(LOCALE_LIST_NODE));

    pNew->pszName = _wcsdup(pszName);
    if (pNew->pszName == NULL)
    {
        free(pNew);
        return NULL;
    }

    pNew->dwId = dwId;

    if (pCurrent == NULL)
    {
        _LocaleList = pNew;
    }
    else
    {
        while (pCurrent->pNext != NULL)
        {
            pCurrent = pCurrent->pNext;
        }

        pNew->pPrev = pCurrent;
        pCurrent->pNext = pNew;
    }

    return pNew;
}


VOID
LocaleList_Destroy(VOID)
{
    LOCALE_LIST_NODE *pCurrent;

    if (_LocaleList == NULL)
        return;

    pCurrent = _LocaleList;

    while (pCurrent != NULL)
    {
        LOCALE_LIST_NODE *pNext = pCurrent->pNext;

        free(pCurrent->pszName);
        free(pCurrent);

        pCurrent = pNext;
    }

    _LocaleList = NULL;
}


static BOOL
IsAllowedKeshOSLang(LANGID langId)
{
    WORD prim = PRIMARYLANGID(langId);
    switch (prim)
    {
        case LANG_ENGLISH:    /* 0x09 */
        case LANG_RUSSIAN:    /* 0x19 */
        case LANG_FRENCH:     /* 0x0c */
        case LANG_GERMAN:     /* 0x07 */
        case LANG_SPANISH:    /* 0x0a */
        case LANG_ITALIAN:    /* 0x10 */
        case LANG_PORTUGUESE: /* 0x16 */
        case LANG_TURKISH:    /* 0x1f */
        case LANG_CZECH:      /* 0x05 */
        case LANG_SWEDISH:    /* 0x1d */
        case LANG_POLISH:     /* 0x15 */
        case LANG_HUNGARIAN:  /* 0x0e */
            return TRUE;
        default:
            return FALSE;
    }
}

LOCALE_LIST_NODE*
LocaleList_Create(VOID)
{
    WCHAR szValue[MAX_PATH];
    DWORD dwIndex;
    DWORD dwSize;
    HKEY hKey;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Nls\\Language",
                      0,
                      KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                      &hKey) != ERROR_SUCCESS)
    {
        return NULL;
    }

    dwSize = ARRAYSIZE(szValue);
    dwIndex = 0;

    while (RegEnumValueW(hKey, dwIndex, szValue, &dwSize,
                         NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
    {
        WCHAR szName[MAX_STR_LEN];
        DWORD dwId;

        dwId = DWORDfromString(szValue);

        if (!IsAllowedKeshOSLang(LOWORD(dwId)))
        {
            dwSize = ARRAYSIZE(szValue);
            ++dwIndex;
            continue;
        }

        if (GetLocaleInfoW(LOWORD(dwId),
                           LOCALE_SLANGUAGE,
                           szName, ARRAYSIZE(szName)))
        {
            LocaleList_Append(dwId, szName);
        }

        dwSize = ARRAYSIZE(szValue);
        ++dwIndex;
    }

    RegCloseKey(hKey);

    return _LocaleList;
}


LOCALE_LIST_NODE*
LocaleList_GetByHkl(HKL hkl)
{
    LOCALE_LIST_NODE *pCurrent;

    for (pCurrent = _LocaleList; pCurrent != NULL; pCurrent = pCurrent->pNext)
    {
        if (LOWORD(pCurrent->dwId) == LOWORD(hkl))
        {
            return pCurrent;
        }
    }

    return NULL;
}


LOCALE_LIST_NODE*
LocaleList_GetFirst(VOID)
{
    return _LocaleList;
}
