﻿// Unicode support by Jim Park -- 08/20/2007

#include "makensisw.h"
#include "update.h"
#include "noclib.h"

#include "jnetlib/httpget.h"
#include "../ExDLL/nsis_tchar.h"

static BOOL update_initialized = FALSE;

static JNL_AsyncDNS *g_dns = NULL;

void InitializeUpdate() {
    if (update_initialized)
        return;

    update_initialized = TRUE;
    JNL::open_socketlib();
    g_dns = new JNL_AsyncDNS();
}

void FinalizeUpdate() {
    if (!update_initialized)
        return;

    delete g_dns;
    JNL::close_socketlib();
}

int getProxyInfo(char *out) {
    DWORD v = 0;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD l = 4;
        DWORD t;
        if (RegQueryValueExA(hKey, "ProxyEnable", NULL, &t, (unsigned char *)&v, &l) == ERROR_SUCCESS && t == REG_DWORD) {
            l = 8192;
            if (RegQueryValueExA(hKey, "ProxyServer", NULL, &t, (unsigned char *)out, &l) != ERROR_SUCCESS || t != REG_SZ) {
                v = 0;
                *out = 0;
            }
        }
        else v = 0;
        out[8192 - 1] = 0;
        RegCloseKey(hKey);
    }
    return v;
}

DWORD CALLBACK UpdateThread(LPVOID v) {
#define RSZ 30
    int len;
    char *response = (char *)GlobalAlloc(GPTR, RSZ);
    char *r;
    char url[300];
    BOOL error = FALSE;
    static char pbuf[8192];
    static char ansiBuf[1024];
    char *p = NULL;
    *response = 0;

    if (getProxyInfo(pbuf))
    {
        p = my_strstrA(pbuf, "http=");
        if (!p) p = pbuf;
        else {
            p += 5;
        }
        char *tp = my_strstrA(p, ";");
        if (tp) *tp = 0;
        char *p2 = my_strstrA(p, "=");
        if (p2) p = 0; // we found the wrong proxy
    }

    InitializeUpdate();

    JNL_HTTPGet *get = new JNL_HTTPGet(g_dns, 8192, (p&&p[0]) ? p : NULL);;
    lstrcpyA(url, NSIS_UPDATE);

#ifdef _UNICODE
    WideCharToMultiByte(CP_ACP, 0, g_sdata.brandingv, -1, ansiBuf, sizeof(ansiBuf), NULL, NULL);
    lstrcatA(url, ansiBuf);
#else
    lstrcatA(url, g_sdata.brandingv);
#endif

    lstrcpyA(response, "");
    get->addheader("User-Agent: MakeNSISw (jnetlib)");
    get->addheader("Accept:*/*");
    get->connect(url);
    while (1) {
        int st = get->run();
        if (st < 0) { error = TRUE; break; }//error
        if (get->get_status() == 2) {
            while (len = get->bytes_available()) {
                char b[RSZ];
                if (len > RSZ) len = RSZ;
                if (lstrlenA(response) + len > RSZ) break;
                len = get->get_bytes(b, len);
                b[len] = 0;
                lstrcatA(response, b);
            }
        }
        if (st == 1) break; //closed
    }
    r = response;
    while (r&&*r) {
        if (*r == '\n') { *r = 0; break; }
        r++;
    }
    if (error) {
        char buf[1000];
        wsprintfA(buf, "检查更新时出了问题，请稍后再试。\n\n错误: %s", get->geterrorstr());
        MessageBoxA(g_sdata.hwnd, buf, "NSIS更新", MB_OK | MB_ICONINFORMATION);
    }
    else if (*response == '1'&&lstrlenA(response) > 2) {
        char buf[200];
        response += 2;
        wsprintfA(buf, "现在有新版的NSIS %s可用，你想现在就下载吗？", response);
        if (MessageBoxA(g_sdata.hwnd, buf, "NSIS更新", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            ShellExecuteA(g_sdata.hwnd, "open", NSIS_DL_URL, NULL, NULL, SW_SHOWNORMAL);
        }
    }
    else if (*response == '2'&&lstrlenA(response) > 2) {
        char buf[200];
        response += 2;
        wsprintfA(buf, "现在有新版的NSIS %s可用，你想现在就下载这个预发布版本吗？", response);
        if (MessageBoxA(g_sdata.hwnd, buf, "NSIS更新", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            ShellExecuteA(g_sdata.hwnd, "open", NSIS_DL_URL, NULL, NULL, SW_SHOWNORMAL);
        }
    }
    else MessageBoxA(g_sdata.hwnd, "目前还没有可用的NSIS更新版本", "NSIS更新", MB_OK | MB_ICONINFORMATION);
    GlobalFree(response);
    delete get;
    EnableMenuItem(g_sdata.menu, IDM_NSISUPDATE, MF_ENABLED);
    return 0;
}

void Update() {
    DWORD dwThreadId;

#ifdef _UNICODE
    MessageBox(g_sdata.hwnd, _T("请检测http://www.scratchpaper.com或论坛看有无更新。"), _T("NSIS更新"), MB_OK | MB_ICONSTOP);
    return;
#endif

    if (my_strstr(g_sdata.brandingv, _T("cvs")))
    {
        MessageBox(g_sdata.hwnd, _T("无法检测每日构建版（测试版）的新版本。要更新，请下载新的每日构建版（测试版）。"), _T("NSIS更新"), MB_OK | MB_ICONSTOP);
        return;
    }

    EnableMenuItem(g_sdata.menu, IDM_NSISUPDATE, MF_GRAYED);
    CloseHandle(CreateThread(NULL, 0, UpdateThread, (LPVOID)NULL, 0, &dwThreadId));
}
