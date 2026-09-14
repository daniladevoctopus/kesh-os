/*
 * PROJECT:     KeshOS NT Out-Of-Box Experience (OOBE)
 * LICENSE:     GPL-2.0-or-later
 * PURPOSE:     Modern Material You / FydeOS styled First-Run Wizard
 * DEVELOPER:   SneakDeak Team
 *
 * Page Flow:
 *   0 = Language Selection (animated greetings)
 *   1 = Welcome
 *   2 = EULA
 *   3 = Network
 *   4 = Theme Selection
 *   5 = Account
 *   6 = Finalizing (progress)
 *   7 = Black Screen (reveal)
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>
#include <iphlpapi.h>
#include <math.h>
#include <stdio.h>
#include <lm.h>
#include "resource.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

/* ── Card dimensions ── */
#define CARD_W 860
#define CARD_H 530

/* ── Colors ── */
#define CLR_BG_TOP       RGB(61, 36, 24)
#define CLR_BG_BOT       RGB(26, 14, 8)
#define CLR_CARD_BODY    RGB(255, 252, 250)
#define CLR_CARD_BORDER  RGB(230, 218, 208)
#define CLR_CARD_SHADOW  RGB(18, 10, 6)
#define CLR_TITLE        RGB(44, 26, 16)
#define CLR_BODY         RGB(90, 75, 65)
#define CLR_MUTED        RGB(140, 110, 95)
#define CLR_ACCENT       RGB(180, 100, 45)
#define CLR_BTN_PRI      RGB(132, 70, 36)
#define CLR_BTN_PRI_P    RGB(108, 56, 28)
#define CLR_BTN_PRI_BRD  RGB(168, 95, 48)
#define CLR_BTN_SEC      RGB(252, 248, 245)
#define CLR_BTN_SEC_P    RGB(232, 222, 214)
#define CLR_BTN_SEC_BRD  RGB(210, 195, 185)
#define CLR_BTN_SEC_TXT  RGB(75, 48, 32)
#define CLR_NET_OK_BG    RGB(242, 252, 244)
#define CLR_NET_OK_BRD   RGB(100, 190, 120)
#define CLR_NET_OK_TXT   RGB(25, 120, 45)
#define CLR_NET_NO_BG    RGB(255, 250, 245)
#define CLR_NET_NO_BRD   RGB(230, 215, 200)
#define CLR_NET_NO_TXT   RGB(145, 80, 35)
#define CLR_STEP_ACTIVE  RGB(180, 100, 45)
#define CLR_STEP_INACT   RGB(210, 200, 192)
#define CLR_BLACK_BG     RGB(10, 8, 6)
#define CLR_WHITE_TXT    RGB(252, 248, 244)
#define CLR_GOLD_TXT     RGB(212, 160, 110)
#define CLR_BRANDING     RGB(180, 150, 130)

/* ── Languages ── */
#define LANG_EN 0
#define LANG_RU 1
#define LANG_FR 2
#define LANG_DE 3
#define LANG_ES 4
#define LANG_IT 5
#define LANG_PT 6
#define LANG_TR 7
#define LANG_CS 8
#define LANG_SV 9
#define LANG_PL 10
#define LANG_HU 11
#define LANG_COUNT 12

/* ── Floating Greetings ── */
#define GREET_COUNT 12

typedef struct {
    LPCWSTR text;
    int     baseX, baseY;    /* base position relative to left panel */
    int     fontSize;
    COLORREF color;
    double  phaseX, phaseY;  /* sine phase offsets */
    double  speedX, speedY;  /* sine frequency multipliers */
    int     ampX, ampY;      /* amplitude in pixels */
} GREETING_ITEM;

typedef BOOL (WINAPI *PFN_ActivateThemeFile)(LPCWSTR pwszFile);

/* ── Globals ── */
static HINSTANCE g_hInstance = NULL;
static HWND g_hWndMain = NULL;
static int g_CurrentPage = 0;
static int g_SelectedLang = LANG_EN;

/* Animation */
static int g_AnimFrame = 0;
static int g_AnimTick = 0;
static int g_LoadProgress = 0;

/* Selections */
static int g_SelectedTheme = 0;
static WCHAR g_NetStatusText[256];
static WCHAR g_NetDetailText[256];
static BOOL g_NetConnected = FALSE;
static char g_NetAdapterIp[64] = "";
static char g_NetAdapterGw[64] = "";
static BOOL g_IsLiveCD = FALSE;
static BOOL g_bUserCreated = FALSE;

/* Bitmaps */
static HBITMAP g_hBmpMascot[4] = { NULL };
static HBITMAP g_hBmpGlobe[4] = { NULL };
static HBITMAP g_hBmpShield = NULL;
static HBITMAP g_hBmpThemeBrownie = NULL;
static HBITMAP g_hBmpThemeClassic = NULL;
static HBITMAP g_hBmpAvatar = NULL;

/* Fonts */
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontBody = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontStep = NULL;
static HFONT g_hFontSmall = NULL;

/* Controls */
static HWND g_hBtnNext = NULL;
static HWND g_hBtnBack = NULL;
static HWND g_hBtnSkip = NULL;
static HWND g_hEdtEula = NULL;
static HWND g_hChkEula = NULL;
static HWND g_hEdtUser = NULL;
static HWND g_hEdtPass = NULL;
static HBRUSH g_hBrCardBody = NULL;

/* Floating greetings data (fitted inside 220px left panel) */
static GREETING_ITEM g_Greetings[GREET_COUNT] = {
    { L"Hello",    15,  15, 28, RGB(190, 105, 42),  0.0, 1.0, 0.08, 0.06, 8,  6  },
    { L"\x041F\x0440\x0438\x0432\x0435\x0442", 45, 75, 24, RGB(160, 85, 50), 1.2, 0.5, 0.07, 0.09, 8,  8  },
    { L"Bonjour",  10, 140, 21, RGB(180, 120, 60),  2.5, 1.8, 0.05, 0.07, 8,  6  },
    { L"Hallo",   105,  35, 19, RGB(200, 140, 70),  0.8, 2.2, 0.09, 0.05, 7,  7  },
    { L"Hola",    110, 110, 22, RGB(170, 95,  55),  3.0, 0.3, 0.06, 0.08, 8,  7  },
    { L"Ciao",     65, 180, 18, RGB(185, 110, 48),  1.5, 3.1, 0.10, 0.06, 7,  6  },
    { L"Ol\x00E1", 90, 220, 20, RGB(195, 130, 55),  2.0, 1.5, 0.07, 0.10, 8,  7  },
    { L"Merhaba",  12, 240, 17, RGB(175, 100, 65),  0.5, 2.8, 0.08, 0.07, 7,  8  },
    { L"Ahoj",    120, 170, 17, RGB(165, 88,  52),  3.5, 0.8, 0.06, 0.09, 6,  6  },
    { L"Hej",      35, 285, 19, RGB(205, 145, 72),  1.8, 3.5, 0.09, 0.05, 8,  7  },
    { L"Witaj",    90,  55, 17, RGB(155, 82,  48),  2.8, 1.2, 0.05, 0.08, 7,  6  },
    { L"Szia",    115, 270, 16, RGB(210, 150, 78),  0.3, 2.0, 0.07, 0.06, 7,  6  },
};


/* ── Localized Strings ── */
/* Index: 0=LangTitle, 1=LangDesc, 2=WelcTitle, 3=WelcDesc, 4=EulaTitle, 5=EulaHint,
   6=NetTitle, 7=NetHint, 8=ThemeTitle, 9=ThemeHint, 10=AccTitle, 11=AccHint,
   12=FinTitle, 13=FinHint, 14=BlackText, 15=BtnNext, 16=BtnBack, 17=BtnSkip,
   18=BtnStart, 19=BtnFinish, 20=StepFmt, 21=LiveTitle, 22=LiveDesc,
   23=AccUser, 24=AccPass, 25=EulaAccept, 26=EulaMustAccept,
   27=NetWired, 28=NetWifi, 29=NetNoWire,
   30=ThemeBrownie, 31=ThemeClassic, 32=ThemeBrownieDesc, 33=ThemeClassicDesc,
   34=ThemeOn */
#define LS_COUNT 35

static LPCWSTR g_Strings[LANG_COUNT][LS_COUNT] = {
    /* ── English (default) ── */
    {
        /* 0  */ L"Choose Your Language",
        /* 1  */ L"Select the display language for KeshOS:",
        /* 2  */ L"Welcome to KeshOS!",
        /* 3  */ L"Welcome to the KeshOS NT 0.8.2 Beta \"Brownie\" setup wizard by SneakDeak Team.\n\n"
                 L"KeshOS combines proven classic NT architecture, ultra-fast responsiveness, and a comfortable visual style.\n\n"
                 L"Click \"Begin Setup\" to configure the system in just a few simple steps.",
        /* 4  */ L"License Agreement",
        /* 5  */ L"Please review the KeshOS and SneakDeak Team license terms:",
        /* 6  */ L"Internet Connection",
        /* 7  */ L"Network interface status:",
        /* 8  */ L"Choose Your Theme",
        /* 9  */ L"Theme is applied instantly when you select a card:",
        /* 10 */ L"User Account",
        /* 11 */ L"Specify the primary system user name:",
        /* 12 */ L"Preparing for first launch...",
        /* 13 */ L"Applying settings and preparing the Desktop...",
        /* 14 */ L"Everything is ready for you!",
        /* 15 */ L"Next  >",
        /* 16 */ L"<  Back",
        /* 17 */ L"Skip",
        /* 18 */ L"Begin Setup  >",
        /* 19 */ L"Finish",
        /* 20 */ L"STEP %d OF 5",
        /* 21 */ L"LiveCD Mode Active",
        /* 22 */ L"Logged in as a guest session with Administrator privileges.\nLocal password creation is not required in LiveCD mode.",
        /* 23 */ L"Username:",
        /* 24 */ L"Password (optional):",
        /* 25 */ L"I accept the KeshOS license agreement terms",
        /* 26 */ L"You must accept the license agreement to continue.",
        /* 27 */ L"Wired Ethernet network connected",
        /* 28 */ L"Wireless Network (Wi-Fi)",
        /* 29 */ L"No wired connection detected.\nYou can connect to a network later.",
        /* 30 */ L"Brownie (Chocolate)",
        /* 31 */ L"Classic NT",
        /* 32 */ L"Warm chocolate style",
        /* 33 */ L"Strict Win2000 style",
        /* 34 */ L"ON",
    },
    /* ── Russian ── */
    {
        /* 0  */ L"\x0412\x044B\x0431\x0435\x0440\x0438\x0442\x0435 \x044F\x0437\x044B\x043A",
        /* 1  */ L"\x0412\x044B\x0431\x0435\x0440\x0438\x0442\x0435 \x044F\x0437\x044B\x043A \x0438\x043D\x0442\x0435\x0440\x0444\x0435\x0439\x0441\x0430 KeshOS:",
        /* 2  */ L"\x0414\x043E\x0431\x0440\x043E \x043F\x043E\x0436\x0430\x043B\x043E\x0432\x0430\x0442\x044C \x0432 KeshOS!",
        /* 3  */ L"\x0412\x0430\x0441 \x043F\x0440\x0438\x0432\x0435\x0442\x0441\x0442\x0432\x0443\x0435\x0442 \x043C\x0430\x0441\x0442\x0435\x0440 \x043D\x0430\x0441\x0442\x0440\x043E\x0439\x043A\x0438 KeshOS NT 0.8.2 Beta \"Brownie\" \x043E\x0442 SneakDeak Team.\n\n"
                 L"KeshOS \x0441\x043E\x0447\x0435\x0442\x0430\x0435\x0442 \x043A\x043B\x0430\x0441\x0441\x0438\x0447\x0435\x0441\x043A\x0443\x044E \x0430\x0440\x0445\x0438\x0442\x0435\x043A\x0442\x0443\x0440\x0443 NT, \x0431\x044B\x0441\x0442\x0440\x044B\x0439 \x043E\x0442\x043A\x043B\x0438\x043A \x0438 \x043A\x043E\x043C\x0444\x043E\x0440\x0442\x043D\x044B\x0439 \x0441\x0442\x0438\x043B\x044C.\n\n"
                 L"\x041D\x0430\x0436\x043C\x0438\x0442\x0435 \x043A\x043D\x043E\x043F\x043A\x0443, \x0447\x0442\x043E\x0431\x044B \x043D\x0430\x0441\x0442\x0440\x043E\x0438\x0442\x044C \x0441\x0438\x0441\x0442\x0435\x043C\x0443.",
        /* 4  */ L"\x041B\x0438\x0446\x0435\x043D\x0437\x0438\x044F \x0438 \x0443\x0441\x043B\x043E\x0432\x0438\x044F",
        /* 5  */ L"\x041E\x0437\x043D\x0430\x043A\x043E\x043C\x044C\x0442\x0435\x0441\x044C \x0441 \x0443\x0441\x043B\x043E\x0432\x0438\x044F\x043C\x0438 KeshOS \x0438 SneakDeak Team:",
        /* 6  */ L"\x041F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x0438\x0435 \x043A \x0438\x043D\x0442\x0435\x0440\x043D\x0435\x0442\x0443",
        /* 7  */ L"\x0421\x0442\x0430\x0442\x0443\x0441 \x0441\x0435\x0442\x0435\x0432\x043E\x0433\x043E \x0438\x043D\x0442\x0435\x0440\x0444\x0435\x0439\x0441\x0430:",
        /* 8  */ L"\x0412\x044B\x0431\x0435\x0440\x0438\x0442\x0435 \x0442\x0435\x043C\x0443 \x043E\x0444\x043E\x0440\x043C\x043B\x0435\x043D\x0438\x044F",
        /* 9  */ L"\x0422\x0435\x043C\x0430 \x043F\x0440\x0438\x043C\x0435\x043D\x044F\x0435\x0442\x0441\x044F \x0441\x0440\x0430\x0437\x0443 \x043F\x0440\x0438 \x0432\x044B\x0431\x043E\x0440\x0435:",
        /* 10 */ L"\x0423\x0447\x0435\x0442\x043D\x0430\x044F \x0437\x0430\x043F\x0438\x0441\x044C",
        /* 11 */ L"\x0423\x043A\x0430\x0436\x0438\x0442\x0435 \x0438\x043C\x044F \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044F:",
        /* 12 */ L"\x041F\x043E\x0434\x0433\x043E\x0442\x043E\x0432\x043A\x0430 \x043A \x043F\x0435\x0440\x0432\x043E\x043C\x0443 \x0437\x0430\x043F\x0443\x0441\x043A\x0443...",
        /* 13 */ L"\x041F\x0440\x0438\x043C\x0435\x043D\x0435\x043D\x0438\x0435 \x043D\x0430\x0441\x0442\x0440\x043E\x0435\x043A \x0438 \x043F\x043E\x0434\x0433\x043E\x0442\x043E\x0432\x043A\x0430...",
        /* 14 */ L"\x0412\x0441\x0435 \x043F\x043E\x0434\x0433\x043E\x0442\x043E\x0432\x043B\x0435\x043D\x043E!",
        /* 15 */ L"\x0414\x0430\x043B\x0435\x0435  >",
        /* 16 */ L"<  \x041D\x0430\x0437\x0430\x0434",
        /* 17 */ L"\x041F\x0440\x043E\x043F\x0443\x0441\x0442\x0438\x0442\x044C",
        /* 18 */ L"\x041D\x0430\x0447\x0430\x0442\x044C  >",
        /* 19 */ L"\x0417\x0430\x0432\x0435\x0440\x0448\x0438\x0442\x044C",
        /* 20 */ L"\x0428\x0410\x0413 %d \x0418\x0417 5",
        /* 21 */ L"\x0420\x0435\x0436\x0438\x043C LiveCD \x0430\x043A\x0442\x0438\x0432\x0435\x043D",
        /* 22 */ L"\x0412\x0445\x043E\x0434 \x043F\x043E\x0434 \x0433\x043E\x0441\x0442\x0435\x043C \x0441 \x043F\x0440\x0430\x0432\x0430\x043C\x0438 \x0410\x0434\x043C\x0438\x043D\x0438\x0441\x0442\x0440\x0430\x0442\x043E\x0440\x0430.\n\x041F\x0430\x0440\x043E\x043B\x044C \x043D\x0435 \x0442\x0440\x0435\x0431\x0443\x0435\x0442\x0441\x044F.",
        /* 23 */ L"\x0418\x043C\x044F \x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x0435\x043B\x044F:",
        /* 24 */ L"\x041F\x0430\x0440\x043E\x043B\x044C (\x043E\x043F\x0446\x0438\x043E\x043D\x0430\x043B\x044C\x043D\x043E):",
        /* 25 */ L"\x042F \x043F\x0440\x0438\x043D\x0438\x043C\x0430\x044E \x0443\x0441\x043B\x043E\x0432\x0438\x044F \x043B\x0438\x0446\x0435\x043D\x0437\x0438\x0438 KeshOS",
        /* 26 */ L"\x041D\x0435\x043E\x0431\x0445\x043E\x0434\x0438\x043C\x043E \x043F\x0440\x0438\x043D\x044F\x0442\x044C \x0443\x0441\x043B\x043E\x0432\x0438\x044F.",
        /* 27 */ L"\x041F\x0440\x043E\x0432\x043E\x0434\x043D\x0430\x044F \x0441\x0435\x0442\x044C Ethernet \x043F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x0430",
        /* 28 */ L"\x0411\x0435\x0441\x043F\x0440\x043E\x0432\x043E\x0434\x043D\x0430\x044F \x0441\x0435\x0442\x044C (Wi-Fi)",
        /* 29 */ L"\x041A\x0430\x0431\x0435\x043B\x044C\x043D\x043E\x0435 \x043F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x0438\x0435 \x043D\x0435 \x043E\x0431\x043D\x0430\x0440\x0443\x0436\x0435\x043D\x043E.\n\x041C\x043E\x0436\x043D\x043E \x043F\x043E\x0434\x043A\x043B\x044E\x0447\x0438\x0442\x044C\x0441\x044F \x043F\x043E\x0437\x0436\x0435.",
        /* 30 */ L"Brownie (\x0428\x043E\x043A\x043E\x043B\x0430\x0434)",
        /* 31 */ L"\x041A\x043B\x0430\x0441\x0441\x0438\x0447\x0435\x0441\x043A\x0430\x044F NT",
        /* 32 */ L"\x0422\x0435\x043F\x043B\x044B\x0439 \x0448\x043E\x043A\x043E\x043B\x0430\x0434\x043D\x044B\x0439 \x0441\x0442\x0438\x043B\x044C",
        /* 33 */ L"\x0421\x0442\x0440\x043E\x0433\x0438\x0439 \x0441\x0442\x0438\x043B\x044C Win2000",
    },
    /* ── French (Français) ── */
    {
        /* 0  */ L"Choisissez votre langue",
        /* 1  */ L"S\x00E9" L"lectionnez la langue d'affichage de KeshOS :",
        /* 2  */ L"Bienvenue sur KeshOS !",
        /* 3  */ L"D\x00E9" L"couvrez la nouvelle g\x00E9" L"n\x00E9" L"ration de KeshOS NT 0.8.2 Beta \x00AB Brownie \x00BB.\n\n"
                 L"Cr\x00E9\x00E9" L" avec passion par SneakDeak Team.\n\n"
                 L"Cliquez sur D\x00E9" L"marrer pour configurer le syst\x00E8" L"me en quelques \x00E9" L"tapes.",
        /* 4  */ L"Contrat de licence",
        /* 5  */ L"Veuillez lire et accepter les conditions d'utilisation :",
        /* 6  */ L"Connexion Internet",
        /* 7  */ L"\x00C9" L"tat de l'interface r\x00E9" L"seau :",
        /* 8  */ L"Choisissez votre th\x00E8" L"me",
        /* 9  */ L"Le th\x00E8" L"me s'applique imm\x00E9" L"diatement lors de la s\x00E9" L"lection :",
        /* 10 */ L"Compte d'utilisateur",
        /* 11 */ L"Indiquez le nom de l'utilisateur principal :",
        /* 12 */ L"Pr\x00E9" L"paration au premier lancement...",
        /* 13 */ L"Application des param\x00E8" L"tres et pr\x00E9" L"paration du Bureau...",
        /* 14 */ L"Tout est pr\x00EA" L"t pour vous !",
        /* 15 */ L"Suivant  >",
        /* 16 */ L"<  Retour",
        /* 17 */ L"Ignorer",
        /* 18 */ L"D\x00E9" L"marrer  >",
        /* 19 */ L"Terminer",
        /* 20 */ L"\x00C9" L"TAPE %d SUR 5",
        /* 21 */ L"Mode LiveCD actif",
        /* 22 */ L"Connect\x00E9" L" en session invit\x00E9" L" avec privil\x00E8" L"ges Administrateur.\nLe mot de passe local n'est pas requis.",
        /* 23 */ L"Nom d'utilisateur :",
        /* 24 */ L"Mot de passe (facultatif) :",
        /* 25 */ L"J'accepte les termes du contrat de licence KeshOS",
        /* 26 */ L"Vous devez accepter le contrat pour continuer.",
        /* 27 */ L"R\x00E9" L"seau c\x00E2" L"bl\x00E9" L" Ethernet connect\x00E9",
        /* 28 */ L"R\x00E9" L"seau sans fil (Wi-Fi)",
        /* 29 */ L"Aucune connexion c\x00E2" L"bl\x00E9" L"e d\x00E9" L"tect\x00E9" L"e.\nVous pourrez vous connecter plus tard.",
        /* 30 */ L"Brownie (Chocolat)",
        /* 31 */ L"Classique NT",
        /* 32 */ L"Style chocolat chaleureux",
        /* 33 */ L"Style classique Win2000",
        /* 34 */ L"ACTIF",
    },
    /* ── German (Deutsch) ── */
    {
        /* 0  */ L"W\x00E4" L"hlen Sie Ihre Sprache",
        /* 1  */ L"W\x00E4" L"hlen Sie die Anzeigesprache f\x00FC" L"r KeshOS:",
        /* 2  */ L"Willkommen bei KeshOS!",
        /* 3  */ L"Erleben Sie die neue Generation von KeshOS NT 0.8.2 Beta \x201E" L"Brownie\x201D.\n\n"
                 L"Mit Leidenschaft entwickelt von SneakDeak Team.\n\n"
                 L"Klicken Sie auf Einrichten, um das System zu konfigurieren.",
        /* 4  */ L"Lizenzvereinbarung",
        /* 5  */ L"Bitte lesen Sie die Lizenzbedingungen von KeshOS:",
        /* 6  */ L"Internetverbindung",
        /* 7  */ L"Status der Netzwerkschnittstelle:",
        /* 8  */ L"W\x00E4" L"hlen Sie Ihr Design",
        /* 9  */ L"Design wird bei Auswahl sofort angewendet:",
        /* 10 */ L"Benutzerkonto",
        /* 11 */ L"Geben Sie den prim\x00E4" L"ren Benutzernamen an:",
        /* 12 */ L"Vorbereitung auf den ersten Start...",
        /* 13 */ L"Einstellungen werden angewendet...",
        /* 14 */ L"Alles ist f\x00FC" L"r Sie bereit!",
        /* 15 */ L"Weiter  >",
        /* 16 */ L"<  Zur\x00FC" L"ck",
        /* 17 */ L"\x00DC" L"berspringen",
        /* 18 */ L"Einrichten  >",
        /* 19 */ L"Fertigstellen",
        /* 20 */ L"SCHRITT %d VON 5",
        /* 21 */ L"LiveCD-Modus aktiv",
        /* 22 */ L"Angemeldet als Gastsitzung mit Administratorrechten.\nKein Kennwort erforderlich.",
        /* 23 */ L"Benutzername:",
        /* 24 */ L"Kennwort (optional):",
        /* 25 */ L"Ich akzeptiere die Lizenzbedingungen von KeshOS",
        /* 26 */ L"Sie m\x00FC" L"ssen zustimmen, um fortzufahren.",
        /* 27 */ L"Kabelgebundenes Ethernet-Netzwerk verbunden",
        /* 28 */ L"Drahtlosnetzwerk (Wi-Fi)",
        /* 29 */ L"Keine Kabelverbindung erkannt.\nSie k\x00F6" L"nnen sich sp\x00E4" L"ter verbinden.",
        /* 30 */ L"Brownie (Schokolade)",
        /* 31 */ L"Klassisches NT",
        /* 32 */ L"Warmer Schokoladenstil",
        /* 33 */ L"Klassischer Win2000-Stil",
        /* 34 */ L"EIN",
    },
    /* ── Spanish (Español) ── */
    {
        /* 0  */ L"Elija su idioma",
        /* 1  */ L"Seleccione el idioma de visualizaci\x00F3" L"n para KeshOS:",
        /* 2  */ L"\x00A1" L"Bienvenido a KeshOS!",
        /* 3  */ L"Descubra la nueva generaci\x00F3" L"n de KeshOS NT 0.8.2 Beta \x00AB Brownie \x00BB.\n\n"
                 L"Creado con pasi\x00F3" L"n por SneakDeak Team.\n\n"
                 L"Haga clic en Iniciar para configurar el sistema en pocos pasos.",
        /* 4  */ L"Acuerdo de licencia",
        /* 5  */ L"Revise los t\x00E9" L"rminos de licencia de KeshOS y SneakDeak Team:",
        /* 6  */ L"Conexi\x00F3" L"n a Internet",
        /* 7  */ L"Estado de la interfaz de red:",
        /* 8  */ L"Elija su tema",
        /* 9  */ L"El tema se aplica de inmediato al seleccionarlo:",
        /* 10 */ L"Cuenta de usuario",
        /* 11 */ L"Especifique el nombre de usuario principal:",
        /* 12 */ L"Preparando el primer inicio...",
        /* 13 */ L"Aplicando configuraci\x00F3" L"n y preparando el escritorio...",
        /* 14 */ L"\x00A1" L"Todo est\x00E1" L" listo para su uso!",
        /* 15 */ L"Siguiente  >",
        /* 16 */ L"<  Atr\x00E1" L"s",
        /* 17 */ L"Omitir",
        /* 18 */ L"Iniciar  >",
        /* 19 */ L"Finalizar",
        /* 20 */ L"PASO %d DE 5",
        /* 21 */ L"Modo LiveCD activo",
        /* 22 */ L"Iniciado como sesi\x00F3" L"n de invitado con permisos de Administrador.\nNo se requiere contrase\x00F1" L"a.",
        /* 23 */ L"Nombre de usuario:",
        /* 24 */ L"Contrase\x00F1" L"a (opcional):",
        /* 25 */ L"Acepto los t\x00E9" L"rminos de la licencia de KeshOS",
        /* 26 */ L"Debe aceptar el acuerdo de licencia para continuar.",
        /* 27 */ L"Red Ethernet por cable conectada",
        /* 28 */ L"Red inal\x00E1" L"mbrica (Wi-Fi)",
        /* 29 */ L"No se detect\x00F3" L" conexi\x00F3" L"n por cable.\nPuede conectarse m\x00E1" L"s tarde.",
        /* 30 */ L"Brownie (Chocolate)",
        /* 31 */ L"Cl\x00E1" L"sico NT",
        /* 32 */ L"Estilo chocolate c\x00E1" L"lido",
        /* 33 */ L"Estilo cl\x00E1" L"sico Win2000",
        /* 34 */ L"ACTIVO",
    },
    /* ── Italian (Italiano) ── */
    {
        /* 0  */ L"Scegli la tua lingua",
        /* 1  */ L"Seleziona la lingua di visualizzazione per KeshOS:",
        /* 2  */ L"Benvenuto in KeshOS!",
        /* 3  */ L"Scopri la nuova generazione di KeshOS NT 0.8.2 Beta \x00AB Brownie \x00BB.\n\n"
                 L"Creato con passione da SneakDeak Team.\n\n"
                 L"Fai clic su Inizia per configurare il sistema in pochi passaggi.",
        /* 4  */ L"Contratto di licenza",
        /* 5  */ L"Leggi i termini di licenza di KeshOS e SneakDeak Team:",
        /* 6  */ L"Connessione Internet",
        /* 7  */ L"Stato interfaccia di rete:",
        /* 8  */ L"Scegli il tuo tema",
        /* 9  */ L"Il tema si applica istantaneamente alla selezione:",
        /* 10 */ L"Account utente",
        /* 11 */ L"Specifica il nome utente principale:",
        /* 12 */ L"Preparazione al primo avvio...",
        /* 13 */ L"Applicazione delle impostazioni e del desktop...",
        /* 14 */ L"Tutto \x00E8" L" pronto per te!",
        /* 15 */ L"Avanti  >",
        /* 16 */ L"<  Indietro",
        /* 17 */ L"Ignora",
        /* 18 */ L"Inizia  >",
        /* 19 */ L"Fine",
        /* 20 */ L"PASSO %d DI 5",
        /* 21 */ L"Modalit\x00E0" L" LiveCD attiva",
        /* 22 */ L"Accesso come ospite con privilegi di amministratore.\nNessuna password richiesta.",
        /* 23 */ L"Nome utente:",
        /* 24 */ L"Password (opzionale):",
        /* 25 */ L"Accetto i termini del contratto di licenza KeshOS",
        /* 26 */ L"\x00C8" L" necessario accettare l'accordo per continuare.",
        /* 27 */ L"Rete Ethernet cablata connessa",
        /* 28 */ L"Rete senza fili (Wi-Fi)",
        /* 29 */ L"Nessuna connessione cablata rilevata.\nPuoi connetterti pi\x00F9" L" tardi.",
        /* 30 */ L"Brownie (Cioccolato)",
        /* 31 */ L"Classico NT",
        /* 32 */ L"Stile cioccolato caldo",
        /* 33 */ L"Stile classico Win2000",
        /* 34 */ L"ATTIVO",
    },
    /* ── Portuguese (Português) ── */
    {
        /* 0  */ L"Escolha o seu idioma",
        /* 1  */ L"Selecione o idioma de exibi\x00E7\x00E3" L"o para o KeshOS:",
        /* 2  */ L"Bem-vindo ao KeshOS!",
        /* 3  */ L"Conhe\x00E7" L"a a nova gera\x00E7\x00E3" L"o do KeshOS NT 0.8.2 Beta \x00AB Brownie \x00BB.\n\n"
                 L"Criado com paix\x00E3" L"o pela SneakDeak Team.\n\n"
                 L"Clique em Iniciar para configurar o sistema em poucos passos.",
        /* 4  */ L"Contrato de Licen\x00E7" L"a",
        /* 5  */ L"Reveja os termos de licen\x00E7" L"a do KeshOS e SneakDeak Team:",
        /* 6  */ L"Liga\x00E7\x00E3" L"o \x00E0" L" Internet",
        /* 7  */ L"Estado da interface de rede:",
        /* 8  */ L"Escolha o seu tema",
        /* 9  */ L"O tema \x00E9" L" aplicado de imediato ao selecionar:",
        /* 10 */ L"Conta de Utilizador",
        /* 11 */ L"Indique o nome de utilizador principal:",
        /* 12 */ L"A preparar para o primeiro arranque...",
        /* 13 */ L"A aplicar defini\x00E7\x00F5" L"es e a preparar o ambiente de trabalho...",
        /* 14 */ L"Tudo pronto para si!",
        /* 15 */ L"Seguinte  >",
        /* 16 */ L"<  Voltar",
        /* 17 */ L"Ignorar",
        /* 18 */ L"Iniciar  >",
        /* 19 */ L"Concluir",
        /* 20 */ L"PASSO %d DE 5",
        /* 21 */ L"Modo LiveCD ativo",
        /* 22 */ L"Sess\x00E3" L"o iniciada como convidado com privil\x00E9" L"gios de administrador.\nPalavra-passe n\x00E3" L"o necess\x00E1" L"ria.",
        /* 23 */ L"Nome de utilizador:",
        /* 24 */ L"Palavra-passe (opcional):",
        /* 25 */ L"Aceito os termos do contrato de licen\x00E7" L"a do KeshOS",
        /* 26 */ L"Tem de aceitar o contrato para continuar.",
        /* 27 */ L"Rede Ethernet com fios ligada",
        /* 28 */ L"Rede sem fios (Wi-Fi)",
        /* 29 */ L"Nenhuma liga\x00E7\x00E3" L"o com fios detetada.\nPode ligar-se mais tarde.",
        /* 30 */ L"Brownie (Chocolate)",
        /* 31 */ L"Cl\x00E1" L"ssico NT",
        /* 32 */ L"Estilo chocolate quente",
        /* 33 */ L"Estilo s\x00F3" L"brio Win2000",
        /* 34 */ L"ATIVO",
    },
    /* ── Turkish (Türkçe) ── */
    {
        /* 0  */ L"Dilinizi Se\x00E7" L"in",
        /* 1  */ L"KeshOS i\x00E7" L"in g\x00F6" L"r\x00FC" L"nt\x00FC" L"leme dilini se\x00E7" L"in:",
        /* 2  */ L"KeshOS'a Ho\x015F" L" Geldiniz!",
        /* 3  */ L"Yeni nesil KeshOS NT 0.8.2 Beta \x201C" L"Brownie\x201D ile tan\x0131\x015F\x0131" L"n.\n\n"
                 L"SneakDeak Team taraf\x0131" L"ndan tutkuyla geli\x015F" L"tirildi.\n\n"
                 L"Sistemi birka\x00E7" L" ad\x0131" L"mda yap\x0131" L"land\x0131" L"rmak i\x00E7" L"in Ba\x015F" L"lat'a t\x0131" L"klay\x0131" L"n.",
        /* 4  */ L"Lisans S\x00F6" L"zle\x015F" L"mesi",
        /* 5  */ L"KeshOS ve SneakDeak Team lisans ko\x015F" L"ullar\x0131" L"n\x0131" L" inceleyin:",
        /* 6  */ L"\x0130" L"nternet Ba\x011F" L"lant\x0131" L"s\x0131",
        /* 7  */ L"A\x011F" L" ba\x011F" L"lant\x0131" L" noktas\x0131" L" durumu:",
        /* 8  */ L"Teman\x0131" L"z\x0131" L" Se\x00E7" L"in",
        /* 9  */ L"Kart\x0131" L" se\x00E7" L"ti\x011F" L"inizde tema an\x0131" L"nda uygulan\x0131" L"r:",
        /* 10 */ L"Kullan\x0131" L"c\x0131" L" Hesab\x0131",
        /* 11 */ L"Birincil sistem kullan\x0131" L"c\x0131" L" ad\x0131" L"n\x0131" L" belirtin:",
        /* 12 */ L"\x0130" L"lk ba\x015F" L"latma i\x00E7" L"in haz\x0131" L"rlan\x0131" L"yor...",
        /* 13 */ L"Ayarlar uygulan\x0131" L"yor ve Masa\x00FC" L"st\x00FC" L" haz\x0131" L"rlan\x0131" L"yor...",
        /* 14 */ L"Her \x015F" L"ey sizin i\x00E7" L"in haz\x0131" L"r!",
        /* 15 */ L"\x0130" L"leri  >",
        /* 16 */ L"<  Geri",
        /* 17 */ L"Atla",
        /* 18 */ L"Ba\x015F" L"lat  >",
        /* 19 */ L"Bitir",
        /* 20 */ L"ADIM %d / 5",
        /* 21 */ L"LiveCD Modu Etkin",
        /* 22 */ L"Y\x00F6" L"netici yetkilerine sahip konuk oturumu a\x00E7\x0131" L"ld\x0131.\nParola gerekli de\x011F" L"ildir.",
        /* 23 */ L"Kullan\x0131" L"c\x0131" L" ad\x0131:",
        /* 24 */ L"Parola (\x0130" L"ste\x011F" L"e ba\x011F" L"l\x0131):",
        /* 25 */ L"KeshOS lisans s\x00F6" L"zle\x015F" L"mesi ko\x015F" L"ullar\x0131" L"n\x0131" L" kabul ediyorum",
        /* 26 */ L"Devam etmek i\x00E7" L"in s\x00F6" L"zle\x015F" L"meyi kabul etmelisiniz.",
        /* 27 */ L"Kablolu Ethernet a\x011F\x0131" L" ba\x011F" L"l\x0131",
        /* 28 */ L"Kablosuz A\x011F" L" (Wi-Fi)",
        /* 29 */ L"Kablolu ba\x011F" L"lant\x0131" L" alg\x0131" L"lanmad\x0131.\nDaha sonra ba\x011F" L"lanabilirsiniz.",
        /* 30 */ L"Brownie (\x00C7" L"ikolata)",
        /* 31 */ L"Klasik NT",
        /* 32 */ L"S\x0131" L"cak \x00E7" L"ikolata stili",
        /* 33 */ L"Sade Win2000 stili",
        /* 34 */ L"A\x00C7" L"IK",
    },
    /* ── Czech (Čeština) ── */
    {
        /* 0  */ L"Vyberte sv\x016F" L"j jazyk",
        /* 1  */ L"Vyberte jazyk zobrazen\x00ED" L" pro KeshOS:",
        /* 2  */ L"V\x00ED" L"tejte v KeshOS!",
        /* 3  */ L"Poznejte novou generaci KeshOS NT 0.8.2 Beta \x201E" L"Brownie\x201D.\n\n"
                 L"Vytvo\x0159" L"eno s l\x00E1" L"skou t\x00FD" L"mem SneakDeak Team.\n\n"
                 L"Klepnut\x00ED" L"m na Za\x010D\x00ED" L"t nakonfigurujte syst\x00E9" L"m v n\x011B" L"kolika kroc\x00ED" L"ch.",
        /* 4  */ L"Licen\x010D" L"n\x00ED" L" ujedn\x00E1" L"n\x00ED",
        /* 5  */ L"P\x0159" L"e\x010D" L"t\x011B" L"te si licen\x010D" L"n\x00ED" L" podm\x00ED" L"nky KeshOS a SneakDeak Team:",
        /* 6  */ L"P\x0159" L"ipojen\x00ED" L" k internetu",
        /* 7  */ L"Stav s\x00ED\x0165" L"ov\x00E9" L"ho rozhran\x00ED:",
        /* 8  */ L"Vyberte motiv",
        /* 9  */ L"Motiv se aplikuje okam\x017E" L"it\x011B" L" p\x0159" L"i v\x00FD" L"b\x011B" L"ru:",
        /* 10 */ L"U\x017E" L"ivatelsk\x00FD" L" \x00FA\x010D" L"et",
        /* 11 */ L"Zadejte jm\x00E9" L"no hlavn\x00ED" L"ho u\x017E" L"ivatele:",
        /* 12 */ L"P\x0159\x00ED" L"prava k prvn\x00ED" L"mu spu\x0161" L"t\x011B" L"n\x00ED...",
        /* 13 */ L"Pou\x017E" L"it\x00ED" L" nastaven\x00ED" L" a p\x0159\x00ED" L"prava plochy...",
        /* 14 */ L"V\x0161" L"e je pro v\x00E1" L"s p\x0159" L"ipraveno!",
        /* 15 */ L"Dal\x0161\x00ED" L"  >",
        /* 16 */ L"<  Zp\x011B" L"t",
        /* 17 */ L"P\x0159" L"esko\x010D" L"it",
        /* 18 */ L"Za\x010D\x00ED" L"t  >",
        /* 19 */ L"Dokon\x010D" L"it",
        /* 20 */ L"KROK %d Z 5",
        /* 21 */ L"Re\x017E" L"im LiveCD aktivn\x00ED",
        /* 22 */ L"P\x0159" L"ihl\x00E1\x0161" L"en jako host s opr\x00E1" L"vn\x011B" L"n\x00ED" L"m Spr\x00E1" L"vce.\nHeslo nen\x00ED" L" vy\x017E" L"adov\x00E1" L"no.",
        /* 23 */ L"U\x017E" L"ivatelsk\x00E9" L" jm\x00E9" L"no:",
        /* 24 */ L"Heslo (voliteln\x00E9):",
        /* 25 */ L"P\x0159" L"ij\x00ED" L"m\x00E1" L"m licen\x010D" L"n\x00ED" L" podm\x00ED" L"nky KeshOS",
        /* 26 */ L"Pro pokra\x010D" L"ov\x00E1" L"n\x00ED" L" mus\x00ED" L"te p\x0159" L"ijmout smlouvu.",
        /* 27 */ L"Dr\x00E1" L"tov\x00E1" L" s\x00ED\x0165" L" Ethernet p\x0159" L"ipojena",
        /* 28 */ L"Bezdr\x00E1" L"tov\x00E1" L" s\x00ED\x0165" L" (Wi-Fi)",
        /* 29 */ L"Nenalezeno kabelov\x00E9" L" p\x0159" L"ipojen\x00ED.\nM\x016F\x017E" L"ete se p\x0159" L"ipojit pozd\x011B" L"ji.",
        /* 30 */ L"Brownie (\x010C" L"okol\x00E1" L"da)",
        /* 31 */ L"Klasick\x00E9" L" NT",
        /* 32 */ L"Tepl\x00FD" L" \x010D" L"okol\x00E1" L"dov\x00FD" L" styl",
        /* 33 */ L"\x010C" L"ist\x00FD" L" styl Win2000",
        /* 34 */ L"ZAP",
    },
    /* ── Swedish (Svenska) ── */
    {
        /* 0  */ L"V\x00E4" L"lj ditt spr\x00E5" L"k",
        /* 1  */ L"V\x00E4" L"lj visningsspr\x00E5" L"k f\x00F6" L"r KeshOS:",
        /* 2  */ L"V\x00E4" L"lkommen till KeshOS!",
        /* 3  */ L"Upplev den nya generationen av KeshOS NT 0.8.2 Beta \x201C" L"Brownie\x201D.\n\n"
                 L"Skapat med passion av SneakDeak Team.\n\n"
                 L"Klicka p\x00E5" L" B\x00F6" L"rja f\x00F6" L"r att konfigurera systemet i n\x00E5" L"gra enkla steg.",
        /* 4  */ L"Licensavtal",
        /* 5  */ L"L\x00E4" L"s licensvillkoren f\x00F6" L"r KeshOS och SneakDeak Team:",
        /* 6  */ L"Internetanslutning",
        /* 7  */ L"N\x00E4" L"tverksgr\x00E4" L"nssnittsstatus:",
        /* 8  */ L"V\x00E4" L"lj ditt tema",
        /* 9  */ L"Temat till\x00E4" L"mpas omedelbart n\x00E4" L"r du v\x00E4" L"ljer ett kort:",
        /* 10 */ L"Anv\x00E4" L"ndarkonto",
        /* 11 */ L"Ange prim\x00E4" L"rt anv\x00E4" L"ndarnamn:",
        /* 12 */ L"F\x00F6" L"rbereder f\x00F6" L"rsta start...",
        /* 13 */ L"Till\x00E4" L"mpar inst\x00E4" L"llningar och f\x00F6" L"rbereder skrivbordet...",
        /* 14 */ L"Allt \x00E4" L"r klart f\x00F6" L"r dig!",
        /* 15 */ L"N\x00E4" L"sta  >",
        /* 16 */ L"<  Tillbaka",
        /* 17 */ L"Hoppa \x00F6" L"ver",
        /* 18 */ L"B\x00F6" L"rja  >",
        /* 19 */ L"Slutf\x00F6" L"r",
        /* 20 */ L"STEG %d AV 5",
        /* 21 */ L"LiveCD-l\x00E4" L"ge aktivt",
        /* 22 */ L"Inloggad som g\x00E4" L"st med administrat\x00F6" L"rsbeh\x00F6" L"righet.\nInget l\x00F6" L"senord kr\x00E4" L"vs.",
        /* 23 */ L"Anv\x00E4" L"ndarnamn:",
        /* 24 */ L"L\x00F6" L"senord (valfritt):",
        /* 25 */ L"Jag godk\x00E4" L"nner licensvillkoren f\x00F6" L"r KeshOS",
        /* 26 */ L"Du m\x00E5" L"ste godk\x00E4" L"nna avtalet f\x00F6" L"r att forts\x00E4" L"tta.",
        /* 27 */ L"Kabelanslutet Ethernet anslutet",
        /* 28 */ L"Tr\x00E5" L"dl\x00F6" L"st n\x00E4" L"tverk (Wi-Fi)",
        /* 29 */ L"Ingen kabelanslutning uppt\x00E4" L"cktes.\nDu kan ansluta senare.",
        /* 30 */ L"Brownie (Choklad)",
        /* 31 */ L"Klassisk NT",
        /* 32 */ L"Varm chokladstil",
        /* 33 */ L"Klassisk Win2000-stil",
        /* 34 */ L"P\x00C5",
    },
    /* ── Polish (Polski) ── */
    {
        /* 0  */ L"Wybierz sw\x00F3" L"j j\x0119" L"zyk",
        /* 1  */ L"Wybierz j\x0119" L"zyk wy\x015B" L"wietlania dla KeshOS:",
        /* 2  */ L"Witamy w KeshOS!",
        /* 3  */ L"Poznaj now\x0105" L" generacj\x0119" L" KeshOS NT 0.8.2 Beta \x201E" L"Brownie\x201D.\n\n"
                 L"Stworzony z pasj\x0105" L" przez SneakDeak Team.\n\n"
                 L"Kliknij Rozpocznij, aby skonfigurowa\x0107" L" system w kilku krokach.",
        /* 4  */ L"Umowa licencyjna",
        /* 5  */ L"Zapoznaj si\x0119" L" z warunkami licencji KeshOS i SneakDeak Team:",
        /* 6  */ L"Po\x0142\x0105" L"czenie z Internetem",
        /* 7  */ L"Stan interfejsu sieciowego:",
        /* 8  */ L"Wybierz sw\x00F3" L"j motyw",
        /* 9  */ L"Motyw jest stosowany natychmiast po wybraniu karty:",
        /* 10 */ L"Konto u\x017C" L"ytkownika",
        /* 11 */ L"Podaj g\x0142\x00F3" L"wn\x0105" L" nazw\x0119" L" u\x017C" L"ytkownika:",
        /* 12 */ L"Przygotowanie do pierwszego uruchomienia...",
        /* 13 */ L"Stosowanie ustawie\x0144" L" i przygotowywanie Pulpitu...",
        /* 14 */ L"Wszystko jest gotowe!",
        /* 15 */ L"Dalej  >",
        /* 16 */ L"<  Wstecz",
        /* 17 */ L"Pomi\x0144",
        /* 18 */ L"Rozpocznij  >",
        /* 19 */ L"Zako\x0144" L"cz",
        /* 20 */ L"KROK %d Z 5",
        /* 21 */ L"Tryb LiveCD aktywny",
        /* 22 */ L"Zalogowano jako go\x015B\x0107" L" z uprawnieniami administratora.\nHas\x0142" L"o nie jest wymagane.",
        /* 23 */ L"Nazwa u\x017C" L"ytkownika:",
        /* 24 */ L"Has\x0142" L"o (opcjonalne):",
        /* 25 */ L"Akceptuj\x0119" L" warunki umowy licencyjnej KeshOS",
        /* 26 */ L"Musisz zaakceptowa\x0107" L" umow\x0119" L", aby kontynuowa\x0107.",
        /* 27 */ L"Przewodowa sie\x0107" L" Ethernet pod\x0142\x0105" L"czona",
        /* 28 */ L"Sie\x0107" L" bezprzewodowa (Wi-Fi)",
        /* 29 */ L"Nie wykryto po\x0142\x0105" L"czenia przewodowego.\nMo\x017C" L"esz po\x0142\x0105" L"czy\x0107" L" si\x0119" L" p\x00F3\x017A" L"niej.",
        /* 30 */ L"Brownie (Czekolada)",
        /* 31 */ L"Klasyczny NT",
        /* 32 */ L"Ciep\x0142" L"y styl czekoladowy",
        /* 33 */ L"Tradycyjny styl Win2000",
        /* 34 */ L"W\x0141\x0104" L"CZ",
    },
    /* ── Hungarian (Magyar) ── */
    {
        /* 0  */ L"V\x00E1" L"lassza ki a nyelvet",
        /* 1  */ L"V\x00E1" L"lassza ki a KeshOS megjelen\x00ED" L"t\x00E9" L"si nyelv\x00E9" L"t:",
        /* 2  */ L"\x00DC" L"dv\x00F6" L"zli a KeshOS!",
        /* 3  */ L"Ismerje meg a KeshOS NT 0.8.2 Beta \x201E" L"Brownie\x201D \x00FA" L"j gener\x00E1" L"ci\x00F3" L"j\x00E1" L"t.\n\n"
                 L"A SneakDeak Team \x00E1" L"ltal fejlesztve.\n\n"
                 L"Kattintson a Kezd\x00E9" L"s gombra a rendszer testreszab\x00E1" L"s\x00E1" L"hoz.",
        /* 4  */ L"Licencszerz\x0151" L"d\x00E9" L"s",
        /* 5  */ L"Olvassa el a KeshOS \x00E9" L"s a SneakDeak Team licencfelt\x00E9" L"teleit:",
        /* 6  */ L"Internetkapcsolat",
        /* 7  */ L"H\x00E1" L"l\x00F3" L"zati interf\x00E9" L"sz \x00E1" L"llapota:",
        /* 8  */ L"V\x00E1" L"lassza ki a t\x00E9" L"m\x00E1" L"t",
        /* 9  */ L"A t\x00E9" L"ma azonnal \x00E9" L"rv\x00E9" L"nyes\x00FC" L"l a kiv\x00E1" L"laszt\x00E1" L"skor:",
        /* 10 */ L"Felhaszn\x00E1" L"l\x00F3" L"i fi\x00F3" L"k",
        /* 11 */ L"Adja meg az els\x0151" L"dleges felhaszn\x00E1" L"l\x00F3" L"nevet:",
        /* 12 */ L"Felk\x00E9" L"sz\x00FC" L"l\x00E9" L"s az els\x0151" L" ind\x00ED" L"t\x00E1" L"sra...",
        /* 13 */ L"Be\x00E1" L"ll\x00ED" L"t\x00E1" L"sok alkalmaz\x00E1" L"sa \x00E9" L"s az Asztal el\x0151" L"k\x00E9" L"sz\x00ED" L"t\x00E9" L"se...",
        /* 14 */ L"Minden k\x00E9" L"szen \x00E1" L"ll!",
        /* 15 */ L"Tov\x00E1" L"bb  >",
        /* 16 */ L"<  Vissza",
        /* 17 */ L"Kihagy\x00E1" L"s",
        /* 18 */ L"Kezd\x00E9" L"s  >",
        /* 19 */ L"Befejez\x00E9" L"s",
        /* 20 */ L"%d. L\x00C9" L"P\x00C9" L"S / 5",
        /* 21 */ L"LiveCD m\x00F3" L"d akt\x00ED" L"v",
        /* 22 */ L"Bejelentkezve vend\x00E9" L"gk\x00E9" L"nt Rendszergazdai jogosults\x00E1" L"gokkal.\nJelsz\x00F3" L" nem sz\x00FC" L"ks\x00E9" L"ges.",
        /* 23 */ L"Felhaszn\x00E1" L"l\x00F3" L"n\x00E9" L"v:",
        /* 24 */ L"Jelsz\x00F3" L" (opcion\x00E1" L"lis):",
        /* 25 */ L"Elfogadom a KeshOS licencszerz\x0151" L"d\x00E9" L"s felt\x00E9" L"teleit",
        /* 26 */ L"A folytat\x00E1" L"shoz el kell fogadnia a szerz\x0151" L"d\x00E9" L"st.",
        /* 27 */ L"Vezet\x00E9" L"kes Ethernet h\x00E1" L"l\x00F3" L"zat csatlakoztatva",
        /* 28 */ L"Vezet\x00E9" L"k n\x00E9" L"lk\x00FC" L"li h\x00E1" L"l\x00F3" L"zat (Wi-Fi)",
        /* 29 */ L"Nem tal\x00E1" L"lhat\x00F3" L" vezet\x00E9" L"kes h\x00E1" L"l\x00F3" L"zat.\nK\x00E9" L"s\x0151" L"bb is csatlakozhat.",
        /* 30 */ L"Brownie (Csokol\x00E1" L"de)",
        /* 31 */ L"Klasszikus NT",
        /* 32 */ L"Meleg csokol\x00E1" L"d\x00E9" L" st\x00ED" L"lus",
        /* 33 */ L"Letisztult Win2000 st\x00ED" L"lus",
        /* 34 */ L"BE",
    }
};

/* Helper: get localized string */
static LPCWSTR LS(int idx)
{
    if (idx < 0 || idx >= LS_COUNT) return L"";
    return g_Strings[g_SelectedLang][idx];
}

/* ── Language names for the selector ── */
static LPCWSTR g_LangNames[LANG_COUNT] = {
    L"English",
    L"\x0420\x0443\x0441\x0441\x043A\x0438\x0439",
    L"Fran\x00E7" L"ais",
    L"Deutsch",
    L"Espa\x00F1" L"ol",
    L"Italiano",
    L"Portugu\x00EA" L"s",
    L"T\x00FC" L"rk\x00E7" L"e",
    L"\x010C" L"e\x0161" L"tina",
    L"Svenska",
    L"Polski",
    L"Magyar"
};

static LPCWSTR g_LangSub[LANG_COUNT] = {
    L"United States",
    L"\x0420\x043E\x0441\x0441\x0438\x044F",
    L"France",
    L"Deutschland",
    L"Espa\x00F1" L"a",
    L"Italia",
    L"Portugal / Brasil",
    L"T\x00FC" L"rkiye",
    L"\x010C" L"esk\x00E1" L" republika",
    L"Sverige",
    L"Polska",
    L"Magyarorsz\x00E1" L"g"
};

/* ═══════════════════════════════════════════════════════════════════ */

typedef NTSTATUS (NTAPI *PFN_NtSetDefaultUILanguage)(LANGID LanguageId);
typedef NTSTATUS (NTAPI *PFN_NtSetDefaultLocale)(BOOLEAN UserProfile, LCID DefaultLocaleId);
typedef LANGID (WINAPI *PFN_SetThreadUILanguage)(LANGID LangId);

static const WCHAR s_EulaEN[] =
    L"KESHOS NT 0.8.2 BETA LICENSE AGREEMENT\r\n"
    L"Developer: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT is developed by SneakDeak Team based on ReactOS open-source components and free software.\r\n\r\n"
    L"2. TERMS OF USE:\r\n"
    L"You are free to use, test, and distribute KeshOS under the GNU GPL v2+ license.\r\n\r\n"
    L"3. DISCLAIMER:\r\n"
    L"The software is provided \"as is\" without any warranties, express or implied.\r\n\r\n"
    L"4. ACKNOWLEDGMENTS:\r\n"
    L"The SneakDeak Team thanks the developer community and open-source project authors.\r\n\r\n"
    L"Enjoy KeshOS Brownie!";

static const WCHAR s_EulaRU[] =
    L"\x041B\x0418\x0426\x0415\x041D\x0417\x0418\x041E\x041D\x041D\x041E\x0415 \x0421\x041E\x0413\x041B\x0410\x0428\x0415\x041D\x0418\x0415 KESHOS NT 0.8.2 BETA\r\n"
    L"\x0420\x0430\x0437\x0440\x0430\x0431\x043E\x0442\x0447\x0438\x043A: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT \x0440\x0430\x0437\x0440\x0430\x0431\x0430\x0442\x044B\x0432\x0430\x0435\x0442\x0441\x044F \x043A\x043E\x043C\x0430\x043D\x0434\x043E\x0439 SneakDeak Team \x043D\x0430 \x043E\x0441\x043D\x043E\x0432\x0435 \x043E\x0442\x043A\x0440\x044B\x0442\x044B\x0445 \x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442\x043E\x0432 ReactOS \x0438 \x0441\x0432\x043E\x0431\x043E\x0434\x043D\x043E\x0433\x043E \x041F\x041E.\r\n\r\n"
    L"2. \x0423\x0421\x041B\x041E\x0412\x0418\x042F \x0418\x0421\x041F\x041E\x041B\x042C\x0417\x041E\x0412\x0410\x041D\x0418\x042F:\r\n"
    L"\x0412\x044B \x043C\x043E\x0436\x0435\x0442\x0435 \x0441\x0432\x043E\x0431\x043E\x0434\x043D\x043E \x0438\x0441\x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x0442\x044C, \x0442\x0435\x0441\x0442\x0438\x0440\x043E\x0432\x0430\x0442\x044C \x0438 \x0440\x0430\x0441\x043F\x0440\x043E\x0441\x0442\x0440\x0430\x043D\x044F\x0442\x044C KeshOS \x0432 \x0441\x043E\x043E\x0442\x0432\x0435\x0442\x0441\x0442\x0432\x0438\x0438 \x0441 \x043B\x0438\x0446\x0435\x043D\x0437\x0438\x0435\x0439 GNU GPL v2+.\r\n\r\n"
    L"3. \x041E\x0422\x041A\x0410\x0417 \x041E\x0422 \x041E\x0411\x042F\x0417\x0410\x0422\x0415\x041B\x042C\x0421\x0422\x0412:\r\n"
    L"\x041F\x0440\x043E\x0433\x0440\x0430\x043C\x043C\x043D\x043E\x0435 \x043E\x0431\x0435\x0441\x043F\x0435\x0447\x0435\x043D\x0438\x0435 \x043F\x0440\x0435\x0434\x043E\x0441\x0442\x0430\x0432\x043B\x044F\x0435\x0442\x0441\x044F \"\x043A\x0430\x043A \x0435\x0441\x0442\x044C\", \x0431\x0435\x0437 \x043A\x0430\x043A\x0438\x0445-\x043B\x0438\x0431\x043E \x044F\x0432\x043D\x044B\x0445 \x0438\x043B\x0438 \x043F\x043E\x0434\x0440\x0430\x0437\x0443\x043C\x0435\x0432\x0430\x0435\x043C\x044B\x0445 \x0433\x0430\x0440\x0430\x043D\x0442\x0438\x0439.\r\n\r\n"
    L"4. \x0411\x041B\x0410\x0413\x041E\x0414\x0410\x0420\x041D\x041E\x0421\x0422\x0418:\r\n"
    L"SneakDeak Team \x0432\x044B\x0440\x0430\x0436\x0430\x0435\x0442 \x0431\x043B\x0430\x0433\x043E\x0434\x0430\x0440\x043D\x043E\x0441\x0442\x044C \x0441\x043E\x043E\x0431\x0449\x0435\x0441\x0442\x0432\x0443 \x0440\x0430\x0437\x0440\x0430\x0431\x043E\x0442\x0447\x0438\x043A\x043E\x0432 \x0438 \x0430\x0432\x0442\x043E\x0440\x0430\x043C \x043E\x0442\x043A\x0440\x044B\x0442\x044B\x0445 \x043F\x0440\x043E\x0435\x043A\x0442\x043E\x0432.\r\n\r\n"
    L"\x041F\x0440\x0438\x044F\x0442\x043D\x043E\x0433\x043E \x0438\x0441\x043F\x043E\x043B\x044C\x0437\x043E\x0432\x0430\x043D\x0438\x044F KeshOS Brownie!";

static const WCHAR s_EulaFR[] =
    L"CONTRAT DE LICENCE KESHOS NT 0.8.2 BETA\r\n"
    L"D\x00E9" L"veloppeur: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT est d\x00E9" L"velopp\x00E9" L" par SneakDeak Team \x00E0" L" partir de composants libres ReactOS.\r\n\r\n"
    L"2. CONDITIONS D'UTILISATION:\r\n"
    L"Vous \x00EA" L"tes libre d'utiliser, tester et redistribuer KeshOS sous licence GNU GPL v2+.\r\n\r\n"
    L"3. EXCLUSION DE GARANTIE:\r\n"
    L"Le logiciel est fourni \"tel quel\", sans aucune garantie expresse ou implicite.\r\n\r\n"
    L"4. REMERCIEMENTS:\r\n"
    L"SneakDeak Team remercie la communaut\x00E9" L" et les auteurs de projets open-source.\r\n\r\n"
    L"Profitez bien de KeshOS Brownie!";

static const WCHAR s_EulaDE[] =
    L"LIZENZVEREINBARUNG KESHOS NT 0.8.2 BETA\r\n"
    L"Entwickler: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT wird vom SneakDeak Team auf Basis von Open-Source-Komponenten entwickelt.\r\n\r\n"
    L"2. NUTZUNGSBEDINGUNGEN:\r\n"
    L"Sie k\x00F6" L"nnen KeshOS gem\x00E4\x00DF" L" der GNU GPL v2+ Lizenz frei nutzen und verteilen.\r\n\r\n"
    L"3. HAFTUNGSAUSSCHLUSS:\r\n"
    L"Die Software wird \"wie besehen\" ohne jegliche Gew\x00E4" L"hrleistung bereitgestellt.\r\n\r\n"
    L"4. DANKSAGUNG:\r\n"
    L"Das SneakDeak Team dankt der Entwickler-Community und den Open-Source-Autoren.\r\n\r\n"
    L"Viel Spa\x00DF" L" mit KeshOS Brownie!";

static const WCHAR s_EulaES[] =
    L"ACUERDO DE LICENCIA KESHOS NT 0.8.2 BETA\r\n"
    L"Desarrollador: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT es desarrollado por SneakDeak Team basado en componentes abiertos ReactOS.\r\n\r\n"
    L"2. T\x00C9" L"RMINOS DE USO:\r\n"
    L"Puede utilizar, probar y distribuir KeshOS bajo la licencia GNU GPL v2+.\r\n\r\n"
    L"3. EXENCI\x00D3" L"N DE RESPONSABILIDAD:\r\n"
    L"El software se proporciona \"tal cual\", sin garant\x00ED" L"as de ning\x00FA" L"n tipo.\r\n\r\n"
    L"4. AGRADECIMIENTOS:\r\n"
    L"SneakDeak Team agradece a la comunidad de desarrolladores de software libre.\r\n\r\n"
    L"\x00A1" L"Disfrute de KeshOS Brownie!";

static const WCHAR s_EulaIT[] =
    L"ACCORDO DI LICENZA KESHOS NT 0.8.2 BETA\r\n"
    L"Sviluppatore: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT \x00E8" L" sviluppato da SneakDeak Team basato su componenti ReactOS.\r\n\r\n"
    L"2. CONDIZIONI D'USO:\r\n"
    L"\x00C8" L" possibile utilizzare, testare e distribuire KeshOS sotto licenza GNU GPL v2+.\r\n\r\n"
    L"3. ESCLUSIONE DI GARANZIA:\r\n"
    L"Il software viene fornito \"cos\x00EC" L" com'\x00E8" L"\", senza garanzie di alcun tipo.\r\n\r\n"
    L"4. RINGRAZIAMENTI:\r\n"
    L"SneakDeak Team ringrazia la comunit\x00E0" L" degli sviluppatori open source.\r\n\r\n"
    L"Buon divertimento con KeshOS Brownie!";

static const WCHAR s_EulaPT[] =
    L"CONTRATO DE LICEN\x00C7" L"A KESHOS NT 0.8.2 BETA\r\n"
    L"Desenvolvedor: SneakDeak Team\r\n\r\n"
    L"1. O KeshOS NT \x00E9" L" desenvolvido pela SneakDeak Team baseado no ReactOS.\r\n\r\n"
    L"2. TERMOS DE USO:\r\n"
    L"Voc\x00EA" L" \x00E9" L" livre para usar, testar e distribuir o KeshOS sob a licen\x00E7" L"a GNU GPL v2+.\r\n\r\n"
    L"3. ISEN\x00C7\x00C3" L"O DE GARANTIA:\r\n"
    L"O software \x00E9" L" fornecido \"como est\x00E1" L"\", sem garantias de qualquer tipo.\r\n\r\n"
    L"4. AGRADECIMENTOS:\r\n"
    L"A SneakDeak Team agradece \x00E0" L" comunidade de desenvolvedores open-source.\r\n\r\n"
    L"Aproveite o KeshOS Brownie!";

static const WCHAR s_EulaTR[] =
    L"KESHOS NT 0.8.2 BETA L\x0130" L"SANS S\x00D6" L"ZLE\x015E" L"MES\x0130" L"\r\n"
    L"Geli\x015F" L"tirici: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT, ReactOS ve a\x00E7\x0131" L"k kaynak bile\x015F" L"enler temelinde SneakDeak Team taraf\x0131" L"ndan geli\x015F" L"tirilmektedir.\r\n\r\n"
    L"2. KULLANIM KO\x015E" L"ULLARI:\r\n"
    L"KeshOS'\x0075" L" GNU GPL v2+ lisans\x0131" L" kapsam\x0131" L"nda \x00FC" L"cretsiz kullanabilir ve da\x011F\x0131" L"tabilirsiniz.\r\n\r\n"
    L"3. SORUMLULUK REDD\x0130" L":\r\n"
    L"Yaz\x0131" L"l\x0131" L"m \"oldu\x011F" L"u gibi\" sa\x011F" L"lanmaktad\x0131" L"r.\r\n\r\n"
    L"4. TE\x015E" L"EKK\x00DC" L"RLER:\r\n"
    L"SneakDeak Team t\x00FC" L"m a\x00E7\x0131" L"k kaynak geli\x015F" L"tiricilerine te\x015F" L"ekk\x00FC" L"r eder.\r\n\r\n"
    L"KeshOS Brownie'nin tad\x0131" L"n\x0131" L" \x00E7\x0131" L"kar\x0131" L"n!";

static const WCHAR s_EulaCS[] =
    L"LICEN\x010C" L"N\x00CD" L" UJEDN\x00C1" L"N\x00CD" L" KESHOS NT 0.8.2 BETA\r\n"
    L"V\x00FD" L"voj\x00E1\x0159" L": SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT vyv\x00ED" L"j\x00ED" L" SneakDeak Team na z\x00E1" L"klad\x011B" L" projektu ReactOS.\r\n\r\n"
    L"2. PODM\x00CD" L"NKY POU\x017D" L"IT\x00CD" L":\r\n"
    L"KeshOS m\x016F\x017E" L"ete voln\x011B" L" pou\x017E\x00ED" L"vat a \x0161\x00ED\x0159" L"it pod licenc\x00ED" L" GNU GPL v2+.\r\n\r\n"
    L"3. Z\x00C1" L"RUKA:\r\n"
    L"Software je poskytov\x00E1" L"n \"tak, jak je\", bez z\x00E1" L"ruk.\r\n\r\n"
    L"4. POD\x011A" L"KOV\x00C1" L"N\x00CD" L":\r\n"
    L"SneakDeak Team d\x011B" L"kuje komunit\x011B" L" otev\x0159" L"en\x00E9" L"ho softwaru.\r\n\r\n"
    L"U\x017E\x00ED" L"vejte si KeshOS Brownie!";

static const WCHAR s_EulaSV[] =
    L"LICENSAVTAL F\x00D6" L"R KESHOS NT 0.8.2 BETA\r\n"
    L"Utvecklare: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT utvecklas av SneakDeak Team baserat p\x00E5" L" ReactOS.\r\n\r\n"
    L"2. ANV\x00C4" L"NDARVILLKOR:\r\n"
    L"Du kan fritt anv\x00E4" L"nda, testa och distribuera KeshOS under GNU GPL v2+ licensen.\r\n\r\n"
    L"3. FRISKRIVNINGSKLAUSUL:\r\n"
    L"Programvaran tillhandah\x00E5" L"lls i befintligt skick utan n\x00E5" L"gra garantier.\r\n\r\n"
    L"4. TACK:\r\n"
    L"SneakDeak Team tackar gemenskapen f\x00F6" L"r \x00F6" L"ppen k\x00E4" L"llkod.\r\n\r\n"
    L"Njut av KeshOS Brownie!";

static const WCHAR s_EulaPL[] =
    L"UMOWA LICENCYJNA KESHOS NT 0.8.2 BETA\r\n"
    L"Tw\x00F3" L"rca: SneakDeak Team\r\n\r\n"
    L"1. KeshOS NT jest rozwijany przez SneakDeak Team w oparciu o projekt ReactOS.\r\n\r\n"
    L"2. WARUNKI KORZYSTANIA:\r\n"
    L"Mo\x017C" L"esz swobodnie u\x017C" L"ywa\x0107" L", testowa\x0107" L" i rozpowszechnia\x0107" L" KeshOS na licencji GNU GPL v2+.\r\n\r\n"
    L"3. ZRZECZENIE SI\x0118" L" ODPOWIEDZIALNO\x015A" L"CI:\r\n"
    L"Oprogramowanie jest dostarczane w stanie \"takim, w jakim jest\".\r\n\r\n"
    L"4. PODZI\x0118" L"KOWANIA:\r\n"
    L"SneakDeak Team dzi\x0119" L"kuje spo\x0142" L"eczno\x015B" L"ci tw\x00F3" L"rc\x00F3" L"w otwartego oprogramowania.\r\n\r\n"
    L"Mi\x0142" L"ego korzystania z KeshOS Brownie!";

static const WCHAR s_EulaHU[] =
    L"KESHOS NT 0.8.2 BETA LICENCSZERZ\x0150" L"D\x00C9" L"S\r\n"
    L"Fejleszt\x0151" L": SneakDeak Team\r\n\r\n"
    L"1. A KeshOS NT-t a SneakDeak Team fejleszti a ny\x00ED" L"lt forr\x00E1" L"sk\x00F3" L"d\x00FA" L" ReactOS alapj\x00E1" L"n.\r\n\r\n"
    L"2. FELHASZN\x00C1" L"L\x00C1" L"SI FELT\x00C9" L"TELEK:\r\n"
    L"A KeshOS szabadon haszn\x00E1" L"lhat\x00F3" L" \x00E9" L"s terjeszthet\x0151" L" a GNU GPL v2+ licenc alatt.\r\n\r\n"
    L"3. FELEL\x0150" L"SS\x00C9" L"GKIZ\x00C1" L"R\x00C1" L"S:\r\n"
    L"A szoftver \"adott \x00E1" L"llapotban\" ker\x00FC" L"l \x00E1" L"tad\x00E1" L"sra.\r\n\r\n"
    L"4. K\x00D6" L"SZ\x00D6" L"NETNYILV\x00C1" L"N\x00CD" L"T\x00C1" L"S:\r\n"
    L"A SneakDeak Team k\x00F6" L"sz\x00F6" L"netet mond a ny\x00ED" L"lt forr\x00E1" L"sk\x00F3" L"d\x00FA" L" k\x00F6" L"z\x00F6" L"ss\x00E9" L"gnek.\r\n\r\n"
    L"\x00C9" L"lvezze a KeshOS Brownie-t!";

static void UpdateEulaText(int langIndex)
{
    if (!g_hEdtEula)
        return;

    LPCWSTR szEula = s_EulaEN;
    switch (langIndex)
    {
        case LANG_EN: szEula = s_EulaEN; break;
        case LANG_RU: szEula = s_EulaRU; break;
        case LANG_FR: szEula = s_EulaFR; break;
        case LANG_DE: szEula = s_EulaDE; break;
        case LANG_ES: szEula = s_EulaES; break;
        case LANG_IT: szEula = s_EulaIT; break;
        case LANG_PT: szEula = s_EulaPT; break;
        case LANG_TR: szEula = s_EulaTR; break;
        case LANG_CS: szEula = s_EulaCS; break;
        case LANG_SV: szEula = s_EulaSV; break;
        case LANG_PL: szEula = s_EulaPL; break;
        case LANG_HU: szEula = s_EulaHU; break;
        default:      szEula = s_EulaEN; break;
    }

    SetWindowTextW(g_hEdtEula, szEula);
    SendMessage(g_hEdtEula, EM_SETSEL, 0, 0);
    SendMessage(g_hEdtEula, EM_SCROLLCARET, 0, 0);
}

static void RefreshNetworkText(void)
{
    if (g_NetConnected)
    {
        wsprintfW(g_NetStatusText, L"%s", LS(27));
        if (g_NetAdapterIp[0] != '\0')
        {
            wsprintfW(g_NetDetailText, L"IP: %hs  |  GW: %hs", g_NetAdapterIp, g_NetAdapterGw);
        }
        else
        {
            wsprintfW(g_NetDetailText, L"%s", LS(27));
        }
    }
    else
    {
        wsprintfW(g_NetStatusText, L"%s", LS(28));
        wsprintfW(g_NetDetailText, L"%s", LS(29));
    }
}

static void RegSetStringVal(HKEY hRoot, LPCWSTR subKey, LPCWSTR valName, LPCWSTR valData)
{
    HKEY hKey;
    if (RegCreateKeyExW(hRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, valName, 0, REG_SZ, (const BYTE*)valData, (lstrlenW(valData) + 1) * sizeof(WCHAR));
        RegCloseKey(hKey);
    }
}

static void SaveLocaleInfoParam(HKEY hRoot, LPCWSTR subKey, LCID lcid, LCTYPE lcType, LPCWSTR valName)
{
    WCHAR buf[128];
    if (GetLocaleInfoW(lcid, lcType, buf, ARRAYSIZE(buf)) > 0)
    {
        RegSetStringVal(hRoot, subKey, valName, buf);
    }
}

static void ApplySelectedTheme(int themeIndex)
{
    WCHAR szThemePath[MAX_PATH];
    GetWindowsDirectoryW(szThemePath, MAX_PATH);
    lstrcatW(szThemePath, L"\\Resources\\Themes\\Brownie\\brownie.msstyles");

    /* 1. Ensure Themes service is started */
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCM)
    {
        SC_HANDLE hSvc = OpenServiceW(hSCM, L"Themes", SERVICE_START | SERVICE_QUERY_STATUS);
        if (hSvc)
        {
            SERVICE_STATUS svcStatus;
            if (QueryServiceStatus(hSvc, &svcStatus) && svcStatus.dwCurrentState != SERVICE_RUNNING)
            {
                StartServiceW(hSvc, 0, NULL);
            }
            CloseServiceHandle(hSvc);
        }
        CloseServiceHandle(hSCM);
    }

    /* 2. Install Theme API Hook if uxtheme is available */
    HMODULE hUx = LoadLibraryW(L"uxtheme.dll");
    if (hUx)
    {
        typedef BOOL (WINAPI *PFN_ThemeHooksInstall)(VOID);
        PFN_ThemeHooksInstall pfnHooks = (PFN_ThemeHooksInstall)GetProcAddress(hUx, (LPCSTR)34);
        if (pfnHooks)
            pfnHooks();
        FreeLibrary(hUx);
    }

    /* 3. Configure ThemeManager in HKCU and HKU\.DEFAULT */
    HKEY hRoots[2] = { HKEY_CURRENT_USER, HKEY_USERS };
    LPCWSTR szRoots[2] = { L"Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager",
                          L".DEFAULT\\Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager" };

    for (int r = 0; r < 2; r++)
    {
        HKEY hKey;
        if (RegCreateKeyExW(hRoots[r], szRoots[r], 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS)
        {
            if (themeIndex == 0)
            {
                LPCWSTR szActive = L"1";
                LPCWSTR szColor = L"NormalColor";
                LPCWSTR szSize = L"NormalSize";
                RegSetValueExW(hKey, L"ThemeActive", 0, REG_SZ, (const BYTE*)szActive, (lstrlenW(szActive) + 1) * sizeof(WCHAR));
                RegSetValueExW(hKey, L"ColorName", 0, REG_SZ, (const BYTE*)szColor, (lstrlenW(szColor) + 1) * sizeof(WCHAR));
                RegSetValueExW(hKey, L"SizeName", 0, REG_SZ, (const BYTE*)szSize, (lstrlenW(szSize) + 1) * sizeof(WCHAR));
                RegSetValueExW(hKey, L"DllName", 0, REG_SZ, (const BYTE*)szThemePath, (lstrlenW(szThemePath) + 1) * sizeof(WCHAR));
            }
            else
            {
                LPCWSTR szActive = L"0";
                RegSetValueExW(hKey, L"ThemeActive", 0, REG_SZ, (const BYTE*)szActive, (lstrlenW(szActive) + 1) * sizeof(WCHAR));
                RegDeleteValueW(hKey, L"ColorName");
                RegDeleteValueW(hKey, L"SizeName");
                RegDeleteValueW(hKey, L"DllName");
            }
            RegCloseKey(hKey);
        }
    }

    /* 4. Directly activate theme file via desk.cpl */
    HMODULE hDesk = LoadLibraryW(L"desk.cpl");
    if (hDesk)
    {
        PFN_ActivateThemeFile pfnActivate = (PFN_ActivateThemeFile)GetProcAddress(hDesk, "ActivateThemeFile");
        if (pfnActivate)
            pfnActivate(themeIndex == 0 ? szThemePath : NULL);
        FreeLibrary(hDesk);
    }

    /* 5. Also invoke Control_RunDLL desk.cpl /Action:ActivateMSTheme via rundll32 for full system propagation */
    WCHAR szCmd[512];
    if (themeIndex == 0)
        wsprintfW(szCmd, L"rundll32.exe shell32.dll,Control_RunDLL desk.cpl,,2 /Action:ActivateMSTheme /file:\"%s\"", szThemePath);
    else
        wsprintfW(szCmd, L"rundll32.exe shell32.dll,Control_RunDLL desk.cpl,,2 /Action:ActivateMSTheme");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessW(NULL, szCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    /* 6. Post theme change notification */
    PostMessageW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0);
}

static void ApplySelectedLanguage(int langIndex)
{
    static const struct {
        LANGID  wLangId;
        LCID    lcid;
        LPCWSTR szLangHex4;
        LPCWSTR szLocaleHex8;
        LPCWSTR szLocaleName;
        LPCWSTR szLanguage;
        LPCWSTR szCountry;
        LPCWSTR szKlid;
    } s_LangData[LANG_COUNT] = {
        { 0x0409, 0x00000409, L"0409", L"00000409", L"en-US", L"ENU", L"1",   L"00000409" }, /* EN */
        { 0x0419, 0x00000419, L"0419", L"00000419", L"ru-RU", L"RUS", L"7",   L"00000419" }, /* RU */
        { 0x040C, 0x0000040C, L"040C", L"0000040C", L"fr-FR", L"FRA", L"33",  L"0000040C" }, /* FR */
        { 0x0407, 0x00000407, L"0407", L"00000407", L"de-DE", L"DEU", L"49",  L"00000407" }, /* DE */
        { 0x0C0A, 0x00000C0A, L"0C0A", L"00000C0A", L"es-ES", L"ESN", L"34",  L"0000040A" }, /* ES */
        { 0x0410, 0x00000410, L"0410", L"00000410", L"it-IT", L"ITA", L"39",  L"00000410" }, /* IT */
        { 0x0816, 0x00000816, L"0816", L"00000816", L"pt-PT", L"PTG", L"351", L"00000816" }, /* PT */
        { 0x041F, 0x0000041F, L"041F", L"0000041F", L"tr-TR", L"TRK", L"90",  L"0000041F" }, /* TR */
        { 0x0405, 0x00000405, L"0405", L"00000405", L"cs-CZ", L"CSY", L"420", L"00000405" }, /* CS */
        { 0x041D, 0x0000041D, L"041D", L"0000041D", L"sv-SE", L"SVE", L"46",  L"0000041D" }, /* SV */
        { 0x0415, 0x00000415, L"0415", L"00000415", L"pl-PL", L"PLK", L"48",  L"00000415" }, /* PL */
        { 0x040E, 0x0000040E, L"040E", L"0000040E", L"hu-HU", L"HUN", L"36",  L"0000040E" }, /* HU */
    };

    if (langIndex < 0 || langIndex >= LANG_COUNT)
        return;

    /* 1. HKCU & HKU\.DEFAULT \ Control Panel\Desktop */
    HKEY hRoots[2] = { HKEY_CURRENT_USER, HKEY_USERS };
    LPCWSTR szDeskRoots[2] = { L"Control Panel\\Desktop", L".DEFAULT\\Control Panel\\Desktop" };

    for (int r = 0; r < 2; r++)
    {
        HKEY hRoot = hRoots[r];
        LPCWSTR szDesk = szDeskRoots[r];
        RegSetStringVal(hRoot, szDesk, L"MultiUILanguageId", s_LangData[langIndex].szLangHex4);
        RegSetStringVal(hRoot, szDesk, L"MUILanguagePending", s_LangData[langIndex].szLangHex4);
        RegSetStringVal(hRoot, szDesk, L"PreferredUILanguages", s_LangData[langIndex].szLocaleName);
        RegSetStringVal(hRoot, szDesk, L"PreferredUILanguagesPending", s_LangData[langIndex].szLocaleName);
    }

    /* 2. HKLM\SYSTEM\CurrentControlSet\Control\NLS\Language */
    RegSetStringVal(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NLS\\Language", L"Default", s_LangData[langIndex].szLangHex4);
    RegSetStringVal(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NLS\\Language", L"InstallLanguage", s_LangData[langIndex].szLangHex4);

    /* 3. HKLM\SYSTEM\CurrentControlSet\Control\NLS\CodePage: ACP & OEMCP */
    WCHAR szCpAnsi[16] = L"", szCpOem[16] = L"", szCpMac[16] = L"";
    if (GetLocaleInfoW(s_LangData[langIndex].lcid, LOCALE_IDEFAULTANSICODEPAGE, szCpAnsi, ARRAYSIZE(szCpAnsi)) > 0 && szCpAnsi[0])
    {
        RegSetStringVal(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NLS\\CodePage", L"ACP", szCpAnsi);
    }
    if (GetLocaleInfoW(s_LangData[langIndex].lcid, LOCALE_IDEFAULTCODEPAGE, szCpOem, ARRAYSIZE(szCpOem)) > 0 && szCpOem[0])
    {
        RegSetStringVal(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NLS\\CodePage", L"OEMCP", szCpOem);
    }
    if (GetLocaleInfoW(s_LangData[langIndex].lcid, LOCALE_IDEFAULTMACCODEPAGE, szCpMac, ARRAYSIZE(szCpMac)) > 0 && szCpMac[0])
    {
        RegSetStringVal(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\NLS\\CodePage", L"MACCP", szCpMac);
    }

    /* 4. HKCU & HKU\.DEFAULT \ Control Panel\International (full regional parameters) */
    LPCWSTR szIntRoots[2] = { L"Control Panel\\International", L".DEFAULT\\Control Panel\\International" };

    for (int r = 0; r < 2; r++)
    {
        HKEY hRoot = hRoots[r];
        LPCWSTR szSub = szIntRoots[r];

        RegSetStringVal(hRoot, szSub, L"Locale", s_LangData[langIndex].szLocaleHex8);
        RegSetStringVal(hRoot, szSub, L"LocaleName", s_LangData[langIndex].szLocaleName);
        RegSetStringVal(hRoot, szSub, L"sLanguage", s_LangData[langIndex].szLanguage);
        RegSetStringVal(hRoot, szSub, L"iCountry", s_LangData[langIndex].szCountry);

        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SCOUNTRY, L"sCountry");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SDECIMAL, L"sDecimal");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_STHOUSAND, L"sThousand");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SLIST, L"sList");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SDATE, L"sDate");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_STIME, L"sTime");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SSHORTDATE, L"sShortDate");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SLONGDATE, L"sLongDate");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_STIMEFORMAT, L"sTimeFormat");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SCURRENCY, L"sCurrency");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_ICURRENCY, L"iCurrency");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_INEGCURR, L"iNegCurr");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_IMEASURE, L"iMeasure");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_IDIGITS, L"iDigits");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_ILZERO, L"iLZero");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_INEGNUMBER, L"iNegNumber");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SNEGATIVESIGN, L"sNegativeSign");
        SaveLocaleInfoParam(hRoot, szSub, s_LangData[langIndex].lcid, LOCALE_SPOSITIVESIGN, L"sPositiveSign");
    }

    /* 5. Keyboard Layout\Preload and Toggle */
    for (int r = 0; r < 2; r++)
    {
        HKEY hRoot = hRoots[r];
        LPCWSTR szPreloadSub = (r == 0) ? L"Keyboard Layout\\Preload" : L".DEFAULT\\Keyboard Layout\\Preload";
        LPCWSTR szToggleSub  = (r == 0) ? L"Keyboard Layout\\Toggle"  : L".DEFAULT\\Keyboard Layout\\Toggle";

        RegSetStringVal(hRoot, szPreloadSub, L"1", s_LangData[langIndex].szKlid);
        if (langIndex != LANG_EN)
        {
            RegSetStringVal(hRoot, szPreloadSub, L"2", L"00000409");
        }
        else
        {
            HKEY hKey;
            if (RegOpenKeyExW(hRoot, szPreloadSub, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
            {
                RegDeleteValueW(hKey, L"2");
                RegCloseKey(hKey);
            }
        }

        /* Set keyboard switch hotkey to Alt+Shift ("1") */
        RegSetStringVal(hRoot, szToggleSub, L"Hotkey", L"1");
    }

    /* Activate keyboard layout in current process */
    LoadKeyboardLayoutW(s_LangData[langIndex].szKlid, KLF_ACTIVATE | KLF_REORDER);

    /* 6. Call NT Syscalls */
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (hNtDll)
    {
        PFN_NtSetDefaultUILanguage pfnNtSetDefaultUILanguage =
            (PFN_NtSetDefaultUILanguage)GetProcAddress(hNtDll, "NtSetDefaultUILanguage");
        PFN_NtSetDefaultLocale pfnNtSetDefaultLocale =
            (PFN_NtSetDefaultLocale)GetProcAddress(hNtDll, "NtSetDefaultLocale");

        if (pfnNtSetDefaultUILanguage)
            pfnNtSetDefaultUILanguage(s_LangData[langIndex].wLangId);
        if (pfnNtSetDefaultLocale)
        {
            pfnNtSetDefaultLocale(TRUE, s_LangData[langIndex].lcid);
            pfnNtSetDefaultLocale(FALSE, s_LangData[langIndex].lcid);
        }
    }

    /* 7. Update current thread */
    SetThreadLocale(s_LangData[langIndex].lcid);
    HMODULE hK32 = GetModuleHandleW(L"kernel32.dll");
    if (hK32)
    {
        PFN_SetThreadUILanguage pfnSetThreadUILang =
            (PFN_SetThreadUILanguage)GetProcAddress(hK32, "SetThreadUILanguage");
        if (pfnSetThreadUILang)
            pfnSetThreadUILang(s_LangData[langIndex].wLangId);
    }

    /* 8. Broadcast setting change */
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"intl",
                        SMTO_ABORTIFHUNG, 500, NULL);
    PostMessageW(HWND_BROADCAST, WM_WININICHANGE, 0, (LPARAM)L"intl");
}


static void CreateOrConfigureUserAccount(LPCWSTR szUser, LPCWSTR szPass)
{
    if (!szUser || !szUser[0])
        return;

    /* 1. Add user with USER_INFO_1 via NetUserAdd */
    USER_INFO_1 ui;
    ZeroMemory(&ui, sizeof(ui));
    ui.usri1_name = (LPWSTR)szUser;
    ui.usri1_password = (LPWSTR)(szPass ? szPass : L"");
    ui.usri1_priv = USER_PRIV_USER;
    ui.usri1_flags = UF_SCRIPT | UF_DONT_EXPIRE_PASSWD | UF_NORMAL_ACCOUNT;

    DWORD dwError = 0;
    NET_API_STATUS nStatus = NetUserAdd(NULL, 1, (LPBYTE)&ui, &dwError);

    if (nStatus == NERR_UserExists || nStatus == ERROR_USER_EXISTS)
    {
        /* If user already exists (e.g. Administrator), update password if provided */
        if (szPass && szPass[0])
        {
            USER_INFO_1003 ui1003;
            ui1003.usri1003_password = (LPWSTR)szPass;
            NetUserSetInfo(NULL, szUser, 1003, (LPBYTE)&ui1003, &dwError);
        }
    }

    /* 2. Add user to local Administrators group */
    LOCALGROUP_MEMBERS_INFO_3 lgmi3;
    ZeroMemory(&lgmi3, sizeof(lgmi3));
    lgmi3.lgrmi3_domainandname = (LPWSTR)szUser;
    NetLocalGroupAddMembers(NULL, L"Administrators", 3, (LPBYTE)&lgmi3, 1);

    /* 3. Also execute net user & net localgroup as reliable fallback */
    WCHAR szCmd[512];
    wsprintfW(szCmd, L"cmd.exe /c net user \"%s\" \"%s\" /add /expires:never /active:yes & net localgroup Administrators \"%s\" /add",
              szUser, szPass ? szPass : L"", szUser);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(NULL, szCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 3000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    /* 4. Configure AutoAdminLogon in Winlogon for the newly created user */
    HKEY hKeyWinlogon;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                      0, KEY_SET_VALUE, &hKeyWinlogon) == ERROR_SUCCESS)
    {
        /* AutoAdminLogon = 0 so the user is prompted for their password on the logon screen */
        RegSetValueExW(hKeyWinlogon, L"AutoAdminLogon", 0, REG_SZ, (const BYTE*)L"0", sizeof(L"0"));
        RegSetValueExW(hKeyWinlogon, L"DefaultUserName", 0, REG_SZ,
                       (const BYTE*)szUser, (lstrlenW(szUser) + 1) * sizeof(WCHAR));
        RegSetValueExW(hKeyWinlogon, L"DefaultPassword", 0, REG_SZ, (const BYTE*)L"", sizeof(L""));
        RegCloseKey(hKeyWinlogon);
    }
    g_bUserCreated = TRUE;
}

static void MarkOOBEConfigured(void)
{
    ApplySelectedLanguage(g_SelectedLang);
    ApplySelectedTheme(g_SelectedTheme);

    DWORD dwVal = 1;
    HKEY hKey;

    /* 1. Mark in HKLM so OOBE only runs ONCE on this machine */
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\SneakDeak\\OOBE", 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, L"Configured", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal));
        RegCloseKey(hKey);
    }

    /* 2. Mark in HKCU */
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SneakDeak\\OOBE", 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, L"Configured", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal));
        RegCloseKey(hKey);
    }

    /* 3. Mark in HKU\\.DEFAULT */
    if (RegCreateKeyExW(HKEY_USERS, L".DEFAULT\\Software\\SneakDeak\\OOBE", 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, L"Configured", 0, REG_DWORD, (const BYTE*)&dwVal, sizeof(dwVal));
        RegCloseKey(hKey);
    }
}

static void DetectNetwork(void)
{
    OutputDebugStringA("OOBE: DetectNetwork entered\n");
    IP_ADAPTER_INFO AdapterInfo[16];
    DWORD dwBufLen = sizeof(AdapterInfo);
    DWORD dwStatus = GetAdaptersInfo(AdapterInfo, &dwBufLen);

    g_NetConnected = FALSE;
    g_NetAdapterIp[0] = '\0';
    g_NetAdapterGw[0] = '\0';

    if (dwStatus == ERROR_SUCCESS)
    {
        PIP_ADAPTER_INFO pAdapter = AdapterInfo;
        while (pAdapter)
        {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || pAdapter->Type == IF_TYPE_IEEE80211)
            {
                if (pAdapter->IpAddressList.IpAddress.String[0] != '\0' &&
                    strcmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0 &&
                    strcmp(pAdapter->IpAddressList.IpAddress.String, "127.0.0.1") != 0)
                {
                    g_NetConnected = TRUE;
                    lstrcpynA(g_NetAdapterIp, pAdapter->IpAddressList.IpAddress.String, sizeof(g_NetAdapterIp));
                    lstrcpynA(g_NetAdapterGw, pAdapter->GatewayList.IpAddress.String, sizeof(g_NetAdapterGw));
                    break;
                }
            }
            pAdapter = pAdapter->Next;
        }
    }

    RefreshNetworkText();
    OutputDebugStringA("OOBE: DetectNetwork completed\n");
}

static DWORD WINAPI DetectNetworkThread(LPVOID lpParam)
{
    OutputDebugStringA("OOBE: DetectNetworkThread started in background\n");
    DetectNetwork();
    if (g_hWndMain)
    {
        InvalidateRect(g_hWndMain, NULL, FALSE);
    }
    return 0;
}

static void DetectLiveCD(void)
{
    g_IsLiveCD = FALSE;
    HKEY hKey;

    /* Check MiniNT key (official ReactOS LiveCD indicator) */
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\MiniNT", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        g_IsLiveCD = TRUE;
        RegCloseKey(hKey);
        return;
    }

    /* Check drive type of SystemRoot */
    WCHAR szSysDrive[4] = L"C:\\";
    WCHAR szWinDir[MAX_PATH];
    if (GetWindowsDirectoryW(szWinDir, MAX_PATH) > 0 && szWinDir[1] == L':')
    {
        szSysDrive[0] = szWinDir[0];
        szSysDrive[1] = L':';
        szSysDrive[2] = L'\\';
        szSysDrive[3] = L'\0';
    }

    UINT driveType = GetDriveTypeW(szSysDrive);
    if (driveType == DRIVE_CDROM)
    {
        g_IsLiveCD = TRUE;
        return;
    }
}


/* ── GDI Helpers ── */

static void DrawRoundedRect(HDC hdc, int x, int y, int w, int h, int radius,
                             COLORREF fillColor, COLORREF borderColor)
{
    HBRUSH hBrush = CreateSolidBrush(fillColor);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderColor);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + w, y + h, radius, radius);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

static void DrawFilledCircle(HDC hdc, int cx, int cy, int r, COLORREF color)
{
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

static void DrawHollowCircle(HDC hdc, int cx, int cy, int r, COLORREF color)
{
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

/* Draw step indicator as sleek smooth GDI dots (no distorted cogs) */
static void DrawStepDots(HDC hdc, int x, int y, int currentStep, int totalSteps)
{
    int spacing = 18;

    for (int i = 1; i <= totalSteps; i++)
    {
        int cx = x + (i - 1) * spacing + 6;
        int cy = y + 7;

        if (i == currentStep)
            DrawFilledCircle(hdc, cx, cy, 5, CLR_ACCENT);
        else if (i < currentStep)
            DrawFilledCircle(hdc, cx, cy, 3, RGB(140, 110, 95));
        else
            DrawFilledCircle(hdc, cx, cy, 3, RGB(220, 205, 195));
    }

    /* Step label: "STEP X OF 5" */
    WCHAR szStep[32];
    wsprintfW(szStep, LS(20), currentStep);
    SelectObject(hdc, g_hFontStep);
    SetTextColor(hdc, CLR_MUTED);
    TextOutW(hdc, x + totalSteps * spacing + 12, y, szStep, lstrlenW(szStep));
}

/* ── Unified Layout Calculation ── */

static void GetCardLayout(int cxScreen, int cyScreen,
                          int *pCardX, int *pCardY, int *pCardW, int *pCardH,
                          int *pLeftX, int *pLeftY, int *pLeftW, int *pLeftH,
                          int *pRightX, int *pRightY, int *pRightW, int *pRightH,
                          int *pBtnY)
{
    int cardW = 760;
    int cardH = 520;
    if (cxScreen < cardW + 16) cardW = cxScreen - 16;
    if (cyScreen < cardH + 16) cardH = cyScreen - 20;

    int cardX = (cxScreen - cardW) / 2;
    int cardY = (cyScreen - cardH) / 2;
    if (cardY < 10) cardY = 10;

    int leftW = 238;
    int leftH = cardH - 36;
    int leftX = cardX + 18;
    int leftY = cardY + 18;

    int rightX = leftX + leftW + 24;
    int rightW = cardX + cardW - rightX - 22;
    int rightY = cardY + 26;
    int rightH = cardH - 52;

    int btnH = 38;
    int btnY = cardY + cardH - 18 - btnH;

    if (pCardX) *pCardX = cardX;
    if (pCardY) *pCardY = cardY;
    if (pCardW) *pCardW = cardW;
    if (pCardH) *pCardH = cardH;
    if (pLeftX) *pLeftX = leftX;
    if (pLeftY) *pLeftY = leftY;
    if (pLeftW) *pLeftW = leftW;
    if (pLeftH) *pLeftH = leftH;
    if (pRightX) *pRightX = rightX;
    if (pRightY) *pRightY = rightY;
    if (pRightW) *pRightW = rightW;
    if (pRightH) *pRightH = rightH;
    if (pBtnY) *pBtnY = btnY;
}

/* ── Control visibility & Positioning ── */

static void UpdateControlsVisibility(HWND hWnd)
{
    int cxScreen = GetSystemMetrics(SM_CXSCREEN);
    int cyScreen = GetSystemMetrics(SM_CYSCREEN);
    int cardX, cardY, cardW, cardH, leftX, leftY, leftW, leftH, rightX, rightY, rightW, rightH, btnY;
    GetCardLayout(cxScreen, cyScreen, &cardX, &cardY, &cardW, &cardH, &leftX, &leftY, &leftW, &leftH, &rightX, &rightY, &rightW, &rightH, &btnY);

    /* Pages 0-5 have Next, pages 1-5 have Back, page 3 has Skip */
    BOOL showNext = (g_CurrentPage >= 0 && g_CurrentPage <= 5);
    BOOL showBack = (g_CurrentPage >= 1 && g_CurrentPage <= 5);
    BOOL showSkip = (g_CurrentPage == 3 && !g_NetConnected);

    ShowWindow(g_hBtnNext, showNext ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnBack, showBack ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBtnSkip, showSkip ? SW_SHOW : SW_HIDE);

    /* Button text based on page */
    if (g_CurrentPage == 0)
        SetWindowTextW(g_hBtnNext, LS(15)); /* "Next >" */
    else if (g_CurrentPage == 1)
        SetWindowTextW(g_hBtnNext, LS(18)); /* "Begin Setup >" */
    else if (g_CurrentPage == 5)
        SetWindowTextW(g_hBtnNext, LS(19)); /* "Finish" */
    else
        SetWindowTextW(g_hBtnNext, LS(15)); /* "Next >" */

    SetWindowTextW(g_hBtnBack, LS(16));
    SetWindowTextW(g_hBtnSkip, LS(17));

    /* Position buttons cleanly inside card */
    if (showNext || showBack || showSkip)
    {
        int btnW = 140;
        int btnH = 38;
        int btnNextX = cardX + cardW - 22 - btnW;
        int btnBackX = btnNextX - 12 - 100;
        int btnSkipX = btnBackX - 12 - 90;

        MoveWindow(g_hBtnNext, btnNextX, btnY, btnW, btnH, TRUE);
        MoveWindow(g_hBtnBack, btnBackX, btnY, 100, btnH, TRUE);
        if (showSkip)
            MoveWindow(g_hBtnSkip, btnSkipX, btnY, 90, btnH, TRUE);
    }

    /* EULA controls */
    BOOL showEula = (g_CurrentPage == 2);
    ShowWindow(g_hEdtEula, showEula ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hChkEula, showEula ? SW_SHOW : SW_HIDE);

    /* Account controls */
    BOOL showAccount = (g_CurrentPage == 5 && !g_IsLiveCD);
    ShowWindow(g_hEdtUser, showAccount ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEdtPass, showAccount ? SW_SHOW : SW_HIDE);

    /* Position EULA and Account controls */
    int contentY = rightY + 68;

    if (showEula)
    {
        MoveWindow(g_hEdtEula, rightX, contentY, rightW, 236, TRUE);
        MoveWindow(g_hChkEula, rightX, contentY + 248, rightW, 26, TRUE);
    }
    if (showAccount)
    {
        int editX = rightX + 160;
        int editW = rightW - 170;
        MoveWindow(g_hEdtUser, editX, contentY + 84, editW, 26, TRUE);
        MoveWindow(g_hEdtPass, editX, contentY + 128, editW, 26, TRUE);
    }
}

/* ── Custom owner-draw button ── */

static void OnDrawCustomButton(LPDRAWITEMSTRUCT dis)
{
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    BOOL isSelected = (dis->itemState & ODS_SELECTED);
    BOOL isNext = (dis->CtlID == IDC_BTN_NEXT);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    /* First, fill with card background to prevent black squares */
    HBRUSH hClearBrush = CreateSolidBrush(CLR_CARD_BODY);
    FillRect(hdc, &rc, hClearBrush);
    DeleteObject(hClearBrush);

    if (isNext)
    {
        COLORREF bg = isSelected ? CLR_BTN_PRI_P : CLR_BTN_PRI;
        DrawRoundedRect(hdc, rc.left, rc.top, w, h, h, bg, CLR_BTN_PRI_BRD);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, g_hFontBold);
    }
    else
    {
        COLORREF bg = isSelected ? CLR_BTN_SEC_P : CLR_BTN_SEC;
        DrawRoundedRect(hdc, rc.left, rc.top, w, h, h, bg, CLR_BTN_SEC_BRD);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_BTN_SEC_TXT);
        SelectObject(hdc, g_hFontBody);
    }

    WCHAR szText[64];
    GetWindowTextW(dis->hwndItem, szText, ARRAYSIZE(szText));
    DrawTextW(hdc, szText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/* ── Painting ── */

static void OnPaint(HWND hWnd)
{
    static BOOL s_FirstPaint = TRUE;
    if (s_FirstPaint)
    {
        OutputDebugStringA("OOBE: First OnPaint called\n");
        s_FirstPaint = FALSE;
    }

    PAINTSTRUCT ps;
    HDC hdcWindow = BeginPaint(hWnd, &ps);

    int cxScreen = GetSystemMetrics(SM_CXSCREEN);
    int cyScreen = GetSystemMetrics(SM_CYSCREEN);

    HDC hdc = CreateCompatibleDC(hdcWindow);
    HBITMAP hBmpMem = CreateCompatibleBitmap(hdcWindow, cxScreen, cyScreen);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdc, hBmpMem);

    /* Solid background fallback */
    RECT rcScreen = { 0, 0, cxScreen, cyScreen };
    HBRUSH hFallbackBrush = CreateSolidBrush(RGB(45, 26, 17));
    FillRect(hdc, &rcScreen, hFallbackBrush);
    DeleteObject(hFallbackBrush);

    /* ── Background: Warm Cocoa Gradient ── */
    {
        TRIVERTEX vtx[2];
        vtx[0].x = 0;       vtx[0].y = 0;
        vtx[0].Red = 0x3d00; vtx[0].Green = 0x2400; vtx[0].Blue = 0x1800; vtx[0].Alpha = 0;
        vtx[1].x = cxScreen; vtx[1].y = cyScreen;
        vtx[1].Red = 0x1a00; vtx[1].Green = 0x0e00; vtx[1].Blue = 0x0800; vtx[1].Alpha = 0;
        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(hdc, vtx, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
    }

    /* Bottom branding */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_BRANDING);
    SelectObject(hdc, g_hFontSmall);
    {
        RECT rcBrand = { 35, cyScreen - 32, 500, cyScreen };
        DrawTextW(hdc, L"KeshOS NT 0.8.2 Beta \"Brownie\"  |  SneakDeak Team", -1, &rcBrand, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    /* ── Central Card ── */
    {
        int cardX, cardY, cardW, cardH, leftX, leftY, leftW, leftH, rightX, rightY, rightW, rightH, btnY;
        GetCardLayout(cxScreen, cyScreen, &cardX, &cardY, &cardW, &cardH, &leftX, &leftY, &leftW, &leftH, &rightX, &rightY, &rightW, &rightH, &btnY);

        /* Shadow */
        DrawRoundedRect(hdc, cardX + 5, cardY + 7, cardW, cardH, 24, CLR_CARD_SHADOW, CLR_CARD_SHADOW);
        /* Card body */
        DrawRoundedRect(hdc, cardX, cardY, cardW, cardH, 24, CLR_CARD_BODY, CLR_CARD_BORDER);

        /* ── Left Column Container (Hero / Illustration) ── */
        DrawRoundedRect(hdc, leftX, leftY, leftW, leftH, 18, RGB(250, 244, 238), RGB(236, 224, 214));

        if (g_CurrentPage == 0)
        {
            /* Animated floating greetings */
            for (int i = 0; i < GREET_COUNT; i++)
            {
                GREETING_ITEM *g = &g_Greetings[i];
                int offX = (int)(sin(g_AnimTick * g->speedX + g->phaseX) * g->ampX);
                int offY = (int)(sin(g_AnimTick * g->speedY + g->phaseY) * g->ampY);

                int drawX = leftX + g->baseX + offX;
                int drawY = leftY + g->baseY + offY;

                HFONT hGreetFont = CreateFontW(-g->fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hOldFont = (HFONT)SelectObject(hdc, hGreetFont);
                SetTextColor(hdc, g->color);
                TextOutW(hdc, drawX, drawY, g->text, lstrlenW(g->text));
                SelectObject(hdc, hOldFont);
                DeleteObject(hGreetFont);
            }
        }
        else
        {
            /* Standard bitmap illustration scaled to 180x180 centered in 238px left container */
            HDC hdcBmp = CreateCompatibleDC(hdc);
            HBITMAP hCurrentBmp = NULL;

            if (g_CurrentPage == 1 || g_CurrentPage == 6)
                hCurrentBmp = g_hBmpMascot[g_AnimFrame % 4];
            else if (g_CurrentPage == 2)
                hCurrentBmp = g_hBmpShield;
            else if (g_CurrentPage == 3)
                hCurrentBmp = g_hBmpGlobe[g_AnimFrame % 4];
            else if (g_CurrentPage == 4)
                hCurrentBmp = (g_SelectedTheme == 0) ? g_hBmpThemeBrownie : g_hBmpThemeClassic;
            else if (g_CurrentPage == 5)
                hCurrentBmp = g_hBmpAvatar;

            int floatY = (int)(sin(g_AnimTick * 0.15) * 4.0);
            int imgW = 180;
            int imgH = 180;
            int imgX = leftX + (leftW - imgW) / 2;
            int imgY = leftY + 110 + floatY;

            if (hCurrentBmp)
            {
                HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcBmp, hCurrentBmp);
                BITMAP bm;
                GetObject(hCurrentBmp, sizeof(bm), &bm);
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                StretchBlt(hdc, imgX, imgY, imgW, imgH, hdcBmp, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                SelectObject(hdcBmp, hOldSrc);
            }
            DeleteDC(hdcBmp);
        }

        /* Subtitle branding at the bottom of left panel */
        SelectObject(hdc, g_hFontSmall);
        SetTextColor(hdc, RGB(180, 155, 140));
        RECT rcLeftBrand = { leftX, leftY + leftH - 28, leftX + leftW, leftY + leftH };
        DrawTextW(hdc, L"KeshOS  -  Brownie 0.8.2", -1, &rcLeftBrand, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        /* ── Right Content Area ── */
        /* Step dots for pages 1-5 */
        if (g_CurrentPage >= 1 && g_CurrentPage <= 5)
        {
            DrawStepDots(hdc, rightX, rightY, g_CurrentPage, 5);
        }

        SelectObject(hdc, g_hFontTitle);
        SetTextColor(hdc, CLR_TITLE);

        /* ── Page 0: Language Selection ── */
        if (g_CurrentPage == 0)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(0), lstrlenW(LS(0)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_BODY);
            TextOutW(hdc, rightX, rightY + 36, LS(1), lstrlenW(LS(1)));

            int lcY = rightY + 68;
            int colW = 220;
            int rowH = 42;
            int gapX = 14;
            int gapY = 10;

            for (int i = 0; i < LANG_COUNT; i++)
            {
                int r = i % 6;
                int c = i / 6;
                int colX = rightX + c * (colW + gapX);
                int colY = lcY + r * (rowH + gapY);

                BOOL isSel = (g_SelectedLang == i);
                COLORREF cBg = isSel ? RGB(255, 245, 237) : RGB(255, 255, 255);
                COLORREF cBrd = isSel ? CLR_ACCENT : RGB(228, 218, 208);

                DrawRoundedRect(hdc, colX, colY, colW, rowH, 10, cBg, cBrd);

                /* Selection indicator circle */
                int circX = colX + 16;
                int circY = colY + (rowH / 2);
                if (isSel)
                {
                    DrawFilledCircle(hdc, circX, circY, 7, CLR_ACCENT);
                    DrawFilledCircle(hdc, circX, circY, 3, RGB(255, 255, 255));
                }
                else
                {
                    DrawHollowCircle(hdc, circX, circY, 6, CLR_STEP_INACT);
                }

                /* Language name */
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, isSel ? RGB(140, 70, 25) : CLR_TITLE);
                TextOutW(hdc, colX + 32, colY + 5, g_LangNames[i], lstrlenW(g_LangNames[i]));

                /* Subtitle */
                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, CLR_MUTED);
                TextOutW(hdc, colX + 32, colY + 22, g_LangSub[i], lstrlenW(g_LangSub[i]));
            }
        }
        /* ── Page 1: Welcome ── */
        else if (g_CurrentPage == 1)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(2), lstrlenW(LS(2)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_MUTED);
            LPCWSTR szSub = (g_SelectedLang == LANG_RU) ?
                L"Быстрая настройка параметров перед началом работы:" :
                L"Configure your system preferences in a few simple steps:";
            TextOutW(hdc, rightX, rightY + 36, szSub, lstrlenW(szSub));

            int featY = rightY + 70;
            int featH = 78;
            int featGap = 12;

            LPCWSTR featTitles[3] = {
                (g_SelectedLang == LANG_RU) ? L"Классическая надёжность NT" : L"Classic NT Reliability",
                (g_SelectedLang == LANG_RU) ? L"Фирменный стиль Brownie" : L"Signature Brownie Design",
                (g_SelectedLang == LANG_RU) ? L"Готовность прямо с диска" : L"Ready Out of the Box"
            };
            LPCWSTR featSubs[3] = {
                (g_SelectedLang == LANG_RU) ? L"Мгновенный отклик ядра, сверхбыстрая загрузка и минимальное потребление ресурсов." : L"Instant kernel response, ultra-fast boot and minimal resource overhead.",
                (g_SelectedLang == LANG_RU) ? L"Тёплая шоколадная палитра, комфортная типографика и плавные скруглённые формы интерфейса." : L"Warm cocoa palette, comfortable typography and modern rounded user interface.",
                (g_SelectedLang == LANG_RU) ? L"Мастер настроит параметры за несколько простых шагов: система готова к работе сразу после запуска." : L"Configure preferences in a few simple steps: the system is ready for work right after boot."
            };

            for (int f = 0; f < 3; f++)
            {
                int fy = featY + f * (featH + featGap);
                DrawRoundedRect(hdc, rightX, fy, rightW, featH, 12, RGB(253, 249, 245), RGB(234, 222, 212));

                /* Number badge in cocoa accent circle */
                int badgeX = rightX + 28;
                int badgeY = fy + (featH / 2);
                DrawFilledCircle(hdc, badgeX, badgeY, 14, CLR_ACCENT);

                WCHAR szNum[4];
                wsprintfW(szNum, L"%d", f + 1);
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, RGB(255, 255, 255));
                RECT rcNum = { badgeX - 12, badgeY - 10, badgeX + 12, badgeY + 10 };
                DrawTextW(hdc, szNum, -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                /* Title */
                SetTextColor(hdc, CLR_TITLE);
                SelectObject(hdc, g_hFontBold);
                TextOutW(hdc, rightX + 54, fy + 12, featTitles[f], lstrlenW(featTitles[f]));

                /* Subtitle multiline */
                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, RGB(110, 90, 80));
                RECT rcSub = { rightX + 54, fy + 34, rightX + rightW - 16, fy + featH - 6 };
                DrawTextW(hdc, featSubs[f], -1, &rcSub, DT_WORDBREAK);
            }
        }
        /* ── Page 2: EULA ── */
        else if (g_CurrentPage == 2)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(4), lstrlenW(LS(4)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_MUTED);
            TextOutW(hdc, rightX, rightY + 36, LS(5), lstrlenW(LS(5)));
        }
        /* ── Page 3: Network ── */
        else if (g_CurrentPage == 3)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(6), lstrlenW(LS(6)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_MUTED);
            TextOutW(hdc, rightX, rightY + 36, LS(7), lstrlenW(LS(7)));

            int contentY = rightY + 68;

            /* Card 1: Connection Status Hero Card */
            int heroH = 105;
            COLORREF cBg = g_NetConnected ? CLR_NET_OK_BG : CLR_NET_NO_BG;
            COLORREF cBrd = g_NetConnected ? CLR_NET_OK_BRD : CLR_NET_NO_BRD;
            DrawRoundedRect(hdc, rightX, contentY, rightW, heroH, 14, cBg, cBrd);

            /* Status icon circle */
            int iconX = rightX + 32;
            int iconY = contentY + (heroH / 2);
            DrawFilledCircle(hdc, iconX, iconY, 18, g_NetConnected ? RGB(225, 247, 230) : RGB(255, 238, 225));
            DrawHollowCircle(hdc, iconX, iconY, 18, g_NetConnected ? RGB(140, 215, 155) : RGB(235, 195, 170));
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, g_NetConnected ? CLR_NET_OK_TXT : CLR_NET_NO_TXT);
            if (g_NetConnected)
            {
                HPEN hPenCheck = CreatePen(PS_SOLID, 2, CLR_NET_OK_TXT);
                HPEN hOldP = (HPEN)SelectObject(hdc, hPenCheck);
                POINT ptCheck[3] = { { iconX - 6, iconY }, { iconX - 2, iconY + 4 }, { iconX + 6, iconY - 4 } };
                Polyline(hdc, ptCheck, 3);
                SelectObject(hdc, hOldP);
                DeleteObject(hPenCheck);
            }
            else
            {
                RECT rcNetIcon = { iconX - 14, iconY - 12, iconX + 14, iconY + 12 };
                DrawTextW(hdc, L"!", -1, &rcNetIcon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            /* Status title & details */
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, g_NetConnected ? CLR_NET_OK_TXT : CLR_NET_NO_TXT);
            TextOutW(hdc, rightX + 64, contentY + 18, g_NetStatusText, lstrlenW(g_NetStatusText));

            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, RGB(80, 70, 60));
            RECT rcNetDet = { rightX + 64, contentY + 44, rightX + rightW - 16, contentY + heroH - 10 };
            DrawTextW(hdc, g_NetDetailText, -1, &rcNetDet, DT_WORDBREAK);

            /* Card 2: Detailed Network Parameters Card */
            int infoY = contentY + heroH + 14;
            int infoH = 155;
            DrawRoundedRect(hdc, rightX, infoY, rightW, infoH, 14, RGB(252, 248, 244), RGB(234, 222, 212));

            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, CLR_TITLE);
            LPCWSTR szInfoTitle = (g_SelectedLang == LANG_RU) ? L"Параметры сетевого стека KeshOS" : L"KeshOS Network Configuration";
            TextOutW(hdc, rightX + 18, infoY + 14, szInfoTitle, lstrlenW(szInfoTitle));

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, RGB(90, 75, 65));

            LPCWSTR szLblProto = (g_SelectedLang == LANG_RU) ? L"Протокол:" : L"Protocol:";
            LPCWSTR szValProto = (g_SelectedLang == LANG_RU) ? L"TCP/IPv4 (DHCP клиент запущен)" : L"TCP/IPv4 (DHCP client active)";
            TextOutW(hdc, rightX + 18, infoY + 42, szLblProto, lstrlenW(szLblProto));
            SetTextColor(hdc, CLR_ACCENT);
            TextOutW(hdc, rightX + 140, infoY + 42, szValProto, lstrlenW(szValProto));

            SetTextColor(hdc, RGB(90, 75, 65));
            LPCWSTR szLblDns = (g_SelectedLang == LANG_RU) ? L"DNS сервера:" : L"DNS Servers:";
            LPCWSTR szValDns = (g_SelectedLang == LANG_RU) ? L"Автоматическое получение от шлюза" : L"Automatic from Gateway";
            TextOutW(hdc, rightX + 18, infoY + 68, szLblDns, lstrlenW(szLblDns));
            TextOutW(hdc, rightX + 140, infoY + 68, szValDns, lstrlenW(szValDns));

            LPCWSTR szLblWork = (g_SelectedLang == LANG_RU) ? L"Рабочая группа:" : L"Workgroup:";
            TextOutW(hdc, rightX + 18, infoY + 94, szLblWork, lstrlenW(szLblWork));
            TextOutW(hdc, rightX + 140, infoY + 94, L"WORKGROUP", 9);

            /* Helper hint */
            SetTextColor(hdc, RGB(140, 115, 100));
            LPCWSTR szHint = (g_SelectedLang == LANG_RU) ?
                L"Вы сможете настроить статический IP или прокси в Панели управления." :
                L"You can configure static IP, adapter, or proxy settings later in Control Panel.";
            RECT rcHint = { rightX + 18, infoY + 122, rightX + rightW - 18, infoY + infoH - 8 };
            DrawTextW(hdc, szHint, -1, &rcHint, DT_WORDBREAK);
        }
        /* ── Page 4: Theme Selection ── */
        else if (g_CurrentPage == 4)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(8), lstrlenW(LS(8)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_MUTED);
            TextOutW(hdc, rightX, rightY + 36, LS(9), lstrlenW(LS(9)));

            int tcY = rightY + 68;
            int tcW = 220;
            int tcH = 280;
            int tcGap = 14;

            /* Card 0: Brownie */
            {
                BOOL isSel = (g_SelectedTheme == 0);
                COLORREF bBg = isSel ? RGB(255, 246, 238) : RGB(255, 255, 255);
                COLORREF bBrd = isSel ? CLR_ACCENT : RGB(228, 218, 208);
                DrawRoundedRect(hdc, rightX, tcY, tcW, tcH, 16, bBg, bBrd);
            }
            /* Card 1: Classic NT */
            {
                BOOL isSel = (g_SelectedTheme == 1);
                COLORREF cBg = isSel ? RGB(244, 248, 255) : RGB(255, 255, 255);
                COLORREF cBrd = isSel ? RGB(60, 110, 200) : RGB(228, 218, 208);
                DrawRoundedRect(hdc, rightX + tcW + tcGap, tcY, tcW, tcH, 16, cBg, cBrd);
            }

            /* Preview thumbnails */
            HDC hdcThumb = CreateCompatibleDC(hdc);
            if (g_hBmpThemeBrownie)
            {
                HBITMAP hOld = (HBITMAP)SelectObject(hdcThumb, g_hBmpThemeBrownie);
                BITMAP bm;
                GetObject(g_hBmpThemeBrownie, sizeof(bm), &bm);
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                StretchBlt(hdc, rightX + 12, tcY + 12, tcW - 24, 140, hdcThumb, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                SelectObject(hdcThumb, hOld);
            }
            if (g_hBmpThemeClassic)
            {
                HBITMAP hOld = (HBITMAP)SelectObject(hdcThumb, g_hBmpThemeClassic);
                BITMAP bm;
                GetObject(g_hBmpThemeClassic, sizeof(bm), &bm);
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                StretchBlt(hdc, rightX + tcW + tcGap + 12, tcY + 12, tcW - 24, 140, hdcThumb, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                SelectObject(hdcThumb, hOld);
            }
            DeleteDC(hdcThumb);

            /* Theme names */
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, (g_SelectedTheme == 0) ? RGB(160, 80, 30) : RGB(60, 50, 45));
            TextOutW(hdc, rightX + 14, tcY + 162, LS(30), lstrlenW(LS(30)));

            SetTextColor(hdc, (g_SelectedTheme == 1) ? RGB(40, 80, 170) : RGB(60, 50, 45));
            TextOutW(hdc, rightX + tcW + tcGap + 14, tcY + 162, LS(31), lstrlenW(LS(31)));

            /* Descriptions */
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, RGB(120, 100, 90));
            RECT rcDesc0 = { rightX + 14, tcY + 188, rightX + tcW - 14, tcY + 236 };
            DrawTextW(hdc, LS(32), -1, &rcDesc0, DT_WORDBREAK);

            RECT rcDesc1 = { rightX + tcW + tcGap + 14, tcY + 188, rightX + 2 * tcW + tcGap - 14, tcY + 236 };
            DrawTextW(hdc, LS(33), -1, &rcDesc1, DT_WORDBREAK);

            /* Active badge */
            {
                int badgeX = (g_SelectedTheme == 0) ? rightX + 14 : rightX + tcW + tcGap + 14;
                COLORREF badgeClr = (g_SelectedTheme == 0) ? RGB(190, 100, 40) : RGB(60, 110, 200);
                DrawRoundedRect(hdc, badgeX, tcY + tcH - 34, 75, 22, 11, badgeClr, badgeClr);
                SetTextColor(hdc, RGB(255, 255, 255));
                SelectObject(hdc, g_hFontSmall);
                RECT rcBadge = { badgeX, tcY + tcH - 34, badgeX + 75, tcY + tcH - 12 };
                DrawTextW(hdc, LS(34), -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }
        /* ── Page 5: Account ── */
        else if (g_CurrentPage == 5)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(10), lstrlenW(LS(10)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_MUTED);

            int contentY = rightY + 68;

            if (g_IsLiveCD)
            {
                TextOutW(hdc, rightX, rightY + 36, L"Режим ознакомления KeshOS LiveCD", 32);

                int liveCardH = 250;
                DrawRoundedRect(hdc, rightX, contentY, rightW, liveCardH, 16, RGB(255, 250, 242), RGB(238, 195, 125));

                /* Header banner inside card */
                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, RGB(160, 80, 20));
                TextOutW(hdc, rightX + 24, contentY + 20, LS(21), lstrlenW(LS(21)));

                SelectObject(hdc, g_hFontBody);
                SetTextColor(hdc, RGB(80, 65, 55));
                RECT rcLive = { rightX + 24, contentY + 48, rightX + rightW - 24, contentY + 110 };
                DrawTextW(hdc, LS(22), -1, &rcLive, DT_WORDBREAK);

                /* 3 checklist points */
                int listY = contentY + 116;
                LPCWSTR szPoints[3] = {
                    (g_SelectedLang == LANG_RU) ? L"Автоматический вход с полными правами Администратора" : L"Automatic logon with full Administrator privileges",
                    (g_SelectedLang == LANG_RU) ? L"Полный доступ к дискам, оборудованию и файловым системам" : L"Full access to local disks, hardware and filesystems",
                    (g_SelectedLang == LANG_RU) ? L"Изменения и настройки сохраняются в памяти RAM-диска" : L"Changes and sessions are safely stored in RAM disk memory"
                };

                for (int p = 0; p < 3; p++)
                {
                    DrawFilledCircle(hdc, rightX + 32, listY + p * 32 + 8, 4, CLR_ACCENT);
                    SetTextColor(hdc, RGB(90, 60, 40));
                    SelectObject(hdc, g_hFontBody);
                    TextOutW(hdc, rightX + 46, listY + p * 32, szPoints[p], lstrlenW(szPoints[p]));
                }
            }
            else
            {
                TextOutW(hdc, rightX, rightY + 36, LS(11), lstrlenW(LS(11)));

                int accCardH = 260;
                DrawRoundedRect(hdc, rightX, contentY, rightW, accCardH, 16, RGB(252, 248, 244), RGB(234, 222, 212));

                /* Avatar icon at top left of card */
                HDC hdcThumb = CreateCompatibleDC(hdc);
                if (g_hBmpAvatar)
                {
                    HBITMAP hOld = (HBITMAP)SelectObject(hdcThumb, g_hBmpAvatar);
                    BITMAP bm;
                    GetObject(g_hBmpAvatar, sizeof(bm), &bm);
                    SetStretchBltMode(hdc, HALFTONE);
                    SetBrushOrgEx(hdc, 0, 0, NULL);
                    StretchBlt(hdc, rightX + 24, contentY + 20, 52, 52, hdcThumb, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                    SelectObject(hdcThumb, hOld);
                }
                DeleteDC(hdcThumb);

                SelectObject(hdc, g_hFontBold);
                SetTextColor(hdc, CLR_TITLE);
                LPCWSTR szAccHeader = (g_SelectedLang == LANG_RU) ? L"Основная учётная запись" : L"Primary User Account";
                TextOutW(hdc, rightX + 88, contentY + 22, szAccHeader, lstrlenW(szAccHeader));

                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, CLR_ACCENT);
                LPCWSTR szAccRole = (g_SelectedLang == LANG_RU) ? L"Администратор компьютера (Administrator)" : L"Computer Administrator";
                TextOutW(hdc, rightX + 88, contentY + 46, szAccRole, lstrlenW(szAccRole));

                /* Labels for input fields */
                SelectObject(hdc, g_hFontBody);
                SetTextColor(hdc, RGB(60, 45, 35));
                TextOutW(hdc, rightX + 24, contentY + 88, LS(23), lstrlenW(LS(23)));
                TextOutW(hdc, rightX + 24, contentY + 132, LS(24), lstrlenW(LS(24)));

                /* Helpful subtext */
                SelectObject(hdc, g_hFontSmall);
                SetTextColor(hdc, RGB(130, 105, 95));
                LPCWSTR szPassHint = (g_SelectedLang == LANG_RU) ?
                    L"Оставьте поле пароля пустым, если хотите входить в систему сразу без запроса пароля." :
                    L"Leave password blank if you prefer instant automatic login without a password prompt.";
                RECT rcPassHint = { rightX + 24, contentY + 180, rightX + rightW - 24, contentY + accCardH - 12 };
                DrawTextW(hdc, szPassHint, -1, &rcPassHint, DT_WORDBREAK);
            }
        }
        /* ── Page 6: Finalizing ── */
        else if (g_CurrentPage == 6)
        {
            TextOutW(hdc, rightX, rightY + 4, LS(12), lstrlenW(LS(12)));
            SelectObject(hdc, g_hFontBody);
            SetTextColor(hdc, CLR_MUTED);

            LPCWSTR szStatus;
            if (g_LoadProgress < 25)
                szStatus = (g_SelectedLang == LANG_RU) ? L"Применение языковых параметров и раскладки..." : L"Applying regional and language settings...";
            else if (g_LoadProgress < 50)
                szStatus = (g_SelectedLang == LANG_RU) ? L"Настройка визуального стиля и темы Brownie..." : L"Configuring Brownie visual theme and appearance...";
            else if (g_LoadProgress < 85)
                szStatus = (g_SelectedLang == LANG_RU) ? L"Инициализация системного профиля пользователя..." : L"Initializing user profile and services...";
            else if (g_LoadProgress < 100)
                szStatus = (g_SelectedLang == LANG_RU) ? L"Подготовка рабочего стола KeshOS..." : L"Preparing the KeshOS desktop...";
            else
                szStatus = (g_SelectedLang == LANG_RU) ? L"Готово! Запуск рабочего стола..." : L"Ready! Starting KeshOS desktop...";

            TextOutW(hdc, rightX, rightY + 36, szStatus, lstrlenW(szStatus));

            int contentY = rightY + 68;
            int finCardH = 240;
            DrawRoundedRect(hdc, rightX, contentY, rightW, finCardH, 16, RGB(252, 248, 244), RGB(234, 222, 212));

            /* Progress bar */
            int pbX = rightX + 24;
            int pbY = contentY + 95;
            int pbW = rightW - 48;
            int pbH = 18;

            DrawRoundedRect(hdc, pbX, pbY, pbW, pbH, 8, RGB(236, 226, 218), RGB(215, 202, 192));
            int fillW = (pbW * g_LoadProgress) / 100;
            if (fillW > 0)
            {
                if (fillW < 8) fillW = 8;
                DrawRoundedRect(hdc, pbX, pbY, fillW, pbH, 8, CLR_ACCENT, RGB(160, 90, 35));
            }

            /* Percentage text */
            WCHAR szPercent[16];
            wsprintfW(szPercent, L"%d%%", g_LoadProgress);
            SelectObject(hdc, g_hFontBold);
            SetTextColor(hdc, CLR_TITLE);
            RECT rcPct = { pbX, pbY - 26, pbX + pbW, pbY - 4 };
            DrawTextW(hdc, szPercent, -1, &rcPct, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE);

            /* Finalizing note */
            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, RGB(130, 105, 95));
            LPCWSTR szFinNote = (g_SelectedLang == LANG_RU) ?
                L"Пожалуйста, подождите. Через несколько секунд откроется рабочий стол KeshOS." :
                L"Please wait. In just a few moments your KeshOS desktop will appear.";
            RECT rcFinNote = { pbX, pbY + pbH + 20, pbX + pbW, contentY + finCardH - 12 };
            DrawTextW(hdc, szFinNote, -1, &rcFinNote, DT_CENTER | DT_WORDBREAK);
        }
    }

    BitBlt(hdcWindow, 0, 0, cxScreen, cyScreen, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, hOldBmp);
    DeleteObject(hBmpMem);
    DeleteDC(hdc);
    EndPaint(hWnd, &ps);
}


/* ═══════════════════════════════════════════════════════════════════ */

static LRESULT CALLBACK OOBEWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            OutputDebugStringA("OOBE: WM_CREATE started\n");
            g_hWndMain = hWnd;

            /* Ensure cursor is visible on OOBE window */
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            ShowCursor(TRUE);

            /* Create fonts */
            g_hFontTitle    = CreateFontW(-23, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontBody     = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontBold     = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontSmall    = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontStep     = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            /* Load bitmaps */
            g_hBmpMascot[0] = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_MASCOT_0));
            g_hBmpMascot[1] = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_MASCOT_1));
            g_hBmpMascot[2] = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_MASCOT_2));
            g_hBmpMascot[3] = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_MASCOT_3));

            g_hBmpGlobe[0]  = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_GLOBE_0));
            g_hBmpGlobe[1]  = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_GLOBE_1));
            g_hBmpGlobe[2]  = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_GLOBE_2));
            g_hBmpGlobe[3]  = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_GLOBE_3));

            g_hBmpShield    = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_EULA_SHIELD));
            g_hBmpAvatar    = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_ACCOUNT_AVATAR));
            g_hBmpThemeBrownie = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_THEME_BROWNIE));
            g_hBmpThemeClassic = LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_THEME_CLASSIC));

            /* Navigation Buttons */
            g_hBtnNext = CreateWindowExW(0, L"BUTTON", LS(15),
                                         WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_BTN_NEXT, g_hInstance, NULL);
            g_hBtnBack = CreateWindowExW(0, L"BUTTON", LS(16),
                                         WS_CHILD | BS_OWNERDRAW,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_BTN_BACK, g_hInstance, NULL);
            g_hBtnSkip = CreateWindowExW(0, L"BUTTON", LS(17),
                                         WS_CHILD | BS_OWNERDRAW,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_BTN_SKIP, g_hInstance, NULL);

            /* EULA Controls */
            g_hEdtEula = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_EDT_EULA, g_hInstance, NULL);
            SendMessage(g_hEdtEula, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            SetWindowTextW(g_hEdtEula,
                L"ЛИЦЕНЗИОННОЕ СОГЛАШЕНИЕ С КОНЕЧНЫМ ПОЛЬЗОВАТЕЛЕМ KESHOS NT 0.8.2 BETA\r\n\r\n"
                L"1. ПРАВА И УСЛОВИЯ\r\n"
                L"KeshOS NT является свободной операционной системой с открытым исходным кодом,\r\n"
                L"распространяемой на условиях лицензии GNU General Public License v2 (GPL-2.0)\r\n"
                L"и лицензий соответствующих компонентов сторонних разработчиков.\r\n\r\n"
                L"2. ОТКАЗ ОТ ОТВЕТСТВЕННОСТИ\r\n"
                L"ПРОГРАММНОЕ ОБЕСПЕЧЕНИЕ ПРЕДОСТАВЛЯЕТСЯ «КАК ЕСТЬ» (AS IS), БЕЗ КАКИХ-ЛИБО\r\n"
                L"ЯВНЫХ ИЛИ ПОДРАЗУМЕВАЕМЫХ ГАРАНТИЙ, ВКЛЮЧАЯ, НО НЕ ОГРАНИЧИВАЯСЬ ГАРАНТИЯМИ\r\n"
                L"ТОВАРНОЙ ПРИГОДНОСТИ ИЛИ СООТВЕТСТВИЯ ОПРЕДЕЛЕННЫМ ЦЕЛЯМ.\r\n\r\n"
                L"3. АВТОРСКИЕ ПРАВА\r\n"
                L"Все товарные знаки и торговые марки принадлежат их соответствующим владельцам.\r\n"
                L"Оформление Brownie и сопутствующие компоненты (c) 2026 SneakDeak Team.\r\n\r\n"
                L"Нажимая кнопку «Далее», вы подтверждаете согласие с условиями лицензии."
            );

            g_hChkEula = CreateWindowExW(0, L"BUTTON", LS(25),
                                         WS_CHILD | BS_AUTOCHECKBOX,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_CHK_EULA, g_hInstance, NULL);
            SendMessage(g_hChkEula, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);

            /* Account Controls */
            g_hEdtUser = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"User",
                                         WS_CHILD | ES_AUTOHSCROLL,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_EDT_USERNAME, g_hInstance, NULL);
            SendMessage(g_hEdtUser, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);

            g_hEdtPass = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | ES_PASSWORD | ES_AUTOHSCROLL,
                                         0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_EDT_PASSWORD, g_hInstance, NULL);
            SendMessage(g_hEdtPass, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);

            DetectLiveCD();
            /* Run network detection in background thread so WM_CREATE never blocks */
            HANDLE hNetTh = CreateThread(NULL, 0, DetectNetworkThread, NULL, 0, NULL);
            if (hNetTh) CloseHandle(hNetTh);

            UpdateEulaText(g_SelectedLang);
            RefreshNetworkText();

            /* Start animation timer for greetings */
            SetTimer(hWnd, IDT_ANIM, 50, NULL);

            /* Background brush for card controls */
            g_hBrCardBody = CreateSolidBrush(CLR_CARD_BODY);

            UpdateControlsVisibility(hWnd);
            OutputDebugStringA("OOBE: WM_CREATE finished\n");
            break;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            HWND hwndStatic = (HWND)lParam;
            if (hwndStatic == g_hChkEula)
            {
                SetBkMode(hdcStatic, TRANSPARENT);
                SetTextColor(hdcStatic, CLR_TITLE);
                return (LRESULT)g_hBrCardBody;
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        {
            HDC hdcEdit = (HDC)wParam;
            SetBkColor(hdcEdit, RGB(255, 255, 255));
            SetTextColor(hdcEdit, CLR_TITLE);
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }

        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON)
            {
                OnDrawCustomButton(dis);
                return TRUE;
            }
            break;
        }

        case WM_TIMER:
        {
            if (wParam == IDT_ANIM)
            {
                g_AnimFrame = (g_AnimFrame + 1) % 4;
                g_AnimTick++;

                /* Keep taskbar hidden while OOBE is active */
                if (g_CurrentPage <= 6)
                {
                    HWND hShellTray = FindWindowW(L"Shell_TrayWnd", NULL);
                    if (hShellTray && IsWindowVisible(hShellTray))
                        ShowWindow(hShellTray, SW_HIDE);
                }

                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (wParam == IDT_LOADER)
            {
                if (g_CurrentPage == 6)
                {
                    g_LoadProgress += 5;
                    if (g_LoadProgress >= 100)
                    {
                        g_LoadProgress = 100;
                        KillTimer(hWnd, IDT_LOADER);
                        InvalidateRect(hWnd, NULL, FALSE);
                        UpdateWindow(hWnd);

                        /* Mark OOBE configured in HKLM registry */
                        MarkOOBEConfigured();

                        /* Restore taskbar visibility */
                        HWND hShellTray = FindWindowW(L"Shell_TrayWnd", NULL);
                        if (hShellTray)
                            ShowWindow(hShellTray, SW_SHOW);

                        Sleep(350);

                        /* Exit cleanly: userinit will now start explorer.exe */
                        DestroyWindow(hWnd);
                        PostQuitMessage(0);
                        return 0;
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_LBUTTONDOWN:
        {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);

            int cxScreen = GetSystemMetrics(SM_CXSCREEN);
            int cyScreen = GetSystemMetrics(SM_CYSCREEN);
            int cardX, cardY, cardW, cardH, leftX, leftY, leftW, leftH, rightX, rightY, rightW, rightH, btnY;
            GetCardLayout(cxScreen, cyScreen, &cardX, &cardY, &cardW, &cardH, &leftX, &leftY, &leftW, &leftH, &rightX, &rightY, &rightW, &rightH, &btnY);

            /* Language card click handling on Page 0 */
            if (g_CurrentPage == 0)
            {
                int lcY = rightY + 68;
                int colW = 220;
                int rowH = 42;
                int gapX = 14;
                int gapY = 10;
                POINT pt = { mx, my };

                for (int i = 0; i < LANG_COUNT; i++)
                {
                    int r = i % 6;
                    int c = i / 6;
                    int colX = rightX + c * (colW + gapX);
                    int colY = lcY + r * (rowH + gapY);
                    RECT rc = { colX, colY, colX + colW, colY + rowH };
                    if (PtInRect(&rc, pt))
                    {
                        g_SelectedLang = i;
                        ApplySelectedLanguage(i);
                        UpdateEulaText(i);
                        RefreshNetworkText();
                        UpdateControlsVisibility(hWnd);
                        SetWindowTextW(g_hChkEula, LS(25));
                        InvalidateRect(hWnd, NULL, FALSE);
                        break;
                    }
                }
            }

            /* Theme card click handling on Page 4 */
            if (g_CurrentPage == 4)
            {
                int tcY = rightY + 68;
                int tcW = 220;
                int tcH = 280;
                int tcGap = 14;
                RECT rcBrownie = { rightX, tcY, rightX + tcW, tcY + tcH };
                RECT rcClassic = { rightX + tcW + tcGap, tcY, rightX + 2 * tcW + tcGap, tcY + tcH };

                POINT pt = { mx, my };
                if (PtInRect(&rcBrownie, pt))
                {
                    g_SelectedTheme = 0;
                    ApplySelectedTheme(0);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                else if (PtInRect(&rcClassic, pt))
                {
                    g_SelectedTheme = 1;
                    ApplySelectedTheme(1);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_COMMAND:
        {
            WORD id = LOWORD(wParam);
            if (id == IDC_BTN_NEXT)
            {
                if (g_CurrentPage == 0)
                {
                    g_CurrentPage = 1; /* Language -> Welcome */
                }
                else if (g_CurrentPage == 1)
                {
                    g_CurrentPage = 2; /* Welcome -> EULA */
                }
                else if (g_CurrentPage == 2)
                {
                    if (SendMessage(g_hChkEula, BM_GETCHECK, 0, 0) == BST_CHECKED)
                    {
                        g_CurrentPage = 3; /* EULA -> Network */
                    }
                    else
                    {
                        MessageBoxW(hWnd, LS(26), L"KeshOS", MB_ICONINFORMATION);
                        return 0;
                    }
                }
                else if (g_CurrentPage == 3)
                {
                    g_CurrentPage = 4; /* Network -> Themes */
                }
                else if (g_CurrentPage == 4)
                {
                    g_CurrentPage = 5; /* Themes -> Account */
                }
                else if (g_CurrentPage == 5)
                {
                    /* Create user account if not in LiveCD mode */
                    if (!g_IsLiveCD)
                    {
                        WCHAR szUser[128] = L"";
                        WCHAR szPass[128] = L"";
                        GetWindowTextW(g_hEdtUser, szUser, ARRAYSIZE(szUser));
                        GetWindowTextW(g_hEdtPass, szPass, ARRAYSIZE(szPass));

                        /* Trim leading spaces */
                        LPWSTR p = szUser;
                        while (*p == L' ') p++;
                        if (*p)
                        {
                            CreateOrConfigureUserAccount(p, szPass);
                        }
                    }

                    /* Start finalization */
                    g_CurrentPage = 6;
                    g_LoadProgress = 0;
                    SetTimer(hWnd, IDT_LOADER, 40, NULL);
                }
                UpdateControlsVisibility(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else if (id == IDC_BTN_BACK)
            {
                if (g_CurrentPage >= 1 && g_CurrentPage <= 5)
                {
                    g_CurrentPage--;
                    UpdateControlsVisibility(hWnd);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            else if (id == IDC_BTN_SKIP)
            {
                if (g_CurrentPage == 3)
                {
                    g_CurrentPage = 4;
                    UpdateControlsVisibility(hWnd);
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_PAINT:
            OnPaint(hWnd);
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_SETCURSOR:
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;

        case WM_DESTROY:
            if (g_hBrCardBody)
            {
                DeleteObject(g_hBrCardBody);
                g_hBrCardBody = NULL;
            }
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════ */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    OutputDebugStringA("OOBE: wWinMain started\n");

    /* Ensure mouse cursor is visible immediately */
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    ShowCursor(TRUE);

    /* Single instance mutex */
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"KeshOS_OOBE_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        OutputDebugStringA("OOBE: Mutex already exists, exiting\n");
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    /* Detect LiveCD environment */
    DetectLiveCD();
    OutputDebugStringA("OOBE: DetectLiveCD complete\n");

    /* Check if already configured (override if /firstboot or /force passed) */
    BOOL bForce = (lpCmdLine && (wcsstr(lpCmdLine, L"/firstboot") != NULL || wcsstr(lpCmdLine, L"/force") != NULL));
    if (!bForce && (lpCmdLine == NULL || lpCmdLine[0] == L'\0'))
    {
        HKEY hKey;
        DWORD dwVal = 0, dwSize = sizeof(dwVal);
        /* Check HKLM first to ensure single run machine-wide */
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\SneakDeak\\OOBE", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueExW(hKey, L"Configured", NULL, NULL, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwVal == 1)
            {
                RegCloseKey(hKey);
                CloseHandle(hMutex);
                return 0;
            }
            RegCloseKey(hKey);
        }

        dwVal = 0;
        dwSize = sizeof(dwVal);
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SneakDeak\\OOBE", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            if (RegQueryValueExW(hKey, L"Configured", NULL, NULL, (LPBYTE)&dwVal, &dwSize) == ERROR_SUCCESS && dwVal == 1)
            {
                RegCloseKey(hKey);
                CloseHandle(hMutex);
                return 0;
            }
            RegCloseKey(hKey);
        }
    }

    g_hInstance = hInstance;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = OOBEWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_OOBE));
    wc.lpszClassName = L"KeshOS_OOBE_Wnd";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    RegisterClassExW(&wc);
    OutputDebugStringA("OOBE: Window class registered\n");

    int cxScreen = GetSystemMetrics(SM_CXSCREEN);
    int cyScreen = GetSystemMetrics(SM_CYSCREEN);
    char szDbg[128];
    wsprintfA(szDbg, "OOBE: Creating window (%dx%d)...\n", cxScreen, cyScreen);
    OutputDebugStringA(szDbg);

    HWND hWnd = CreateWindowExW(WS_EX_TOPMOST, L"KeshOS_OOBE_Wnd", L"KeshOS OOBE",
                                WS_POPUP | WS_VISIBLE,
                                0, 0, cxScreen, cyScreen,
                                NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        OutputDebugStringA("OOBE: CreateWindowExW failed!\n");
        CloseHandle(hMutex);
        return 0;
    }

    OutputDebugStringA("OOBE: Window created, showing...\n");
    ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);
    UpdateWindow(hWnd);
    OutputDebugStringA("OOBE: Entering message loop\n");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    OutputDebugStringA("OOBE: Message loop exited\n");
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
