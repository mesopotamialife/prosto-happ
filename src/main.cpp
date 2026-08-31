#define UNICODE
#define _UNICODE
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

static const wchar_t* HAPP_URL =
    L"https://github.com/Happ-proxy/happ-desktop/releases/latest/download/setup-Happ.x64.exe";

static void ErrorBox(const std::wstring& s) {
    MessageBoxW(nullptr, s.c_str(), L"Prosto Happ", MB_OK | MB_ICONERROR);
}
static bool Exists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static std::wstring TempPath() {
    wchar_t d[MAX_PATH]{}, f[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, d) || !GetTempFileNameW(d, L"HAP", 0, f)) return L"";
    DeleteFileW(f);
    return f;
}
static bool Download(const std::wstring& url, const std::wstring& out) {
    URL_COMPONENTS u{}; u.dwStructSize = sizeof(u);
    wchar_t host[512]{}, path[4096]{}, extra[4096]{};
    u.lpszHostName=host; u.dwHostNameLength=512;
    u.lpszUrlPath=path; u.dwUrlPathLength=4096;
    u.lpszExtraInfo=extra; u.dwExtraInfoLength=4096;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) return false;

    HINTERNET s=WinHttpOpen(L"Prosto-Happ/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;
    DWORD tls=WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof(tls));

    HINTERNET c=WinHttpConnect(s, host, u.nPort, 0);
    if (!c) { WinHttpCloseHandle(s); return false; }
    std::wstring obj=std::wstring(path)+extra;
    DWORD flags=(u.nScheme==INTERNET_SCHEME_HTTPS)?WINHTTP_FLAG_SECURE:0;
    HINTERNET r=WinHttpOpenRequest(c,L"GET",obj.c_str(),nullptr,
        WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,flags);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }

    bool ok=false;
    if (WinHttpSendRequest(r,WINHTTP_NO_ADDITIONAL_HEADERS,0,
        WINHTTP_NO_REQUEST_DATA,0,0,0) && WinHttpReceiveResponse(r,nullptr)) {
        DWORD status=0, sz=sizeof(status);
        if (WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,&status,&sz,WINHTTP_NO_HEADER_INDEX)
            && status>=200 && status<300) {
            HANDLE h=CreateFileW(out.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,nullptr);
            if (h!=INVALID_HANDLE_VALUE) {
                ok=true; BYTE b[65536]; DWORD n=0,w=0;
                while (WinHttpReadData(r,b,sizeof(b),&n) && n) {
                    if (!WriteFile(h,b,n,&w,nullptr) || w!=n) { ok=false; break; }
                }
                CloseHandle(h);
            }
        }
    }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return ok;
}
static void KillHapp() {
    STARTUPINFOW si{}; PROCESS_INFORMATION pi{}; si.cb=sizeof(si);
    wchar_t cmd[]=L"taskkill.exe /F /IM Happ.exe /T";
    if (CreateProcessW(nullptr,cmd,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) {
        WaitForSingleObject(pi.hProcess,5000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    Sleep(500);
}
static bool RunInstaller(const std::wstring& file) {
    std::wstring args=L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS";
    SHELLEXECUTEINFOW x{}; x.cbSize=sizeof(x); x.fMask=SEE_MASK_NOCLOSEPROCESS;
    x.lpFile=file.c_str(); x.lpParameters=args.c_str(); x.nShow=SW_HIDE;
    if (!ShellExecuteExW(&x)) return false;
    WaitForSingleObject(x.hProcess,INFINITE);
    DWORD code=1; GetExitCodeProcess(x.hProcess,&code); CloseHandle(x.hProcess);
    return code==0;
}
static void S(HKEY root,const wchar_t* key,const wchar_t* name,const wchar_t* value) {
    HKEY h=nullptr;
    if (RegCreateKeyExW(root,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&h,nullptr)==ERROR_SUCCESS) {
        RegSetValueExW(h,name,0,REG_SZ,(const BYTE*)value,
            (DWORD)((wcslen(value)+1)*sizeof(wchar_t)));
        RegCloseKey(h);
    }
}
static void D(HKEY root,const wchar_t* key,const wchar_t* name,DWORD value) {
    HKEY h=nullptr;
    if (RegCreateKeyExW(root,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&h,nullptr)==ERROR_SUCCESS) {
        RegSetValueExW(h,name,0,REG_DWORD,(const BYTE*)&value,sizeof(value));
        RegCloseKey(h);
    }
}
static void Configure() {
    const wchar_t* b=L"Software\\Happ\\OrganizationDefaults\\Preferences";
    S(HKEY_CURRENT_USER,b,L"language",L"ru"); S(HKEY_CURRENT_USER,b,L"mode",L"Light");
    S(HKEY_CURRENT_USER,b,L"fontScale",L"1");

    const wchar_t* a=L"Software\\Happ\\OrganizationDefaults\\Preferences\\AdvancedSettings";
    S(HKEY_CURRENT_USER,a,L"autoStart",L"true"); S(HKEY_CURRENT_USER,a,L"tun",L"false");
    D(HKEY_CURRENT_USER,a,L"tunMode",0); S(HKEY_CURRENT_USER,a,L"tunProvider",L"sing-box");

    const wchar_t* p=L"Software\\Happ\\OrganizationDefaults\\Preferences\\PingSettings";
    S(HKEY_CURRENT_USER,p,L"pingTestUrl",L"http://www.gstatic.com/generate_204");
    D(HKEY_CURRENT_USER,p,L"pingType",1); S(HKEY_CURRENT_USER,p,L"proxyPingMode",L"double");

    const wchar_t* sub=L"Software\\Happ\\OrganizationDefaults\\Preferences\\Subscriptions";
    S(HKEY_CURRENT_USER,sub,L"subsSendHWID",L"true");
    S(HKEY_CURRENT_USER,sub,L"subsAlternativeHWID",L"true");
    S(HKEY_CURRENT_USER,sub,L"subsUpdateOnOpen",L"true");
    S(HKEY_CURRENT_USER,sub,L"subsAutoUpdate",L"true");
    S(HKEY_CURRENT_USER,sub,L"subsReadOnlyHWID",L"true");

    const wchar_t* r=L"Software\\Happ\\OrganizationDefaults\\Preferences\\TunnelSettings\\Routing";
    S(HKEY_CURRENT_USER,r,L"geoFilesUserAgent",L"firefox-win");
}
static void ConfigureChrome() {
    S(HKEY_CURRENT_USER,L"Software\\Policies\\Google\\Chrome",
      L"ProxySettings",L"{\"ProxyMode\":\"system\"}");
}
static std::wstring FindHapp() {
    std::vector<std::wstring> v={
        L"C:\\Program Files\\Happ\\Happ.exe",
        L"C:\\Program Files\\FlyFrogLLC\\Happ\\Happ.exe",
        L"C:\\Program Files (x86)\\Happ\\Happ.exe",
        L"C:\\Program Files (x86)\\FlyFrogLLC\\Happ\\Happ.exe"};
    wchar_t local[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))) {
        v.push_back(std::wstring(local)+L"\\Happ\\Happ.exe");
        v.push_back(std::wstring(local)+L"\\Programs\\Happ\\Happ.exe");
    }
    for (const auto& p:v) if (Exists(p)) return p;
    return L"";
}
static bool Is64() {
#if defined(_WIN64)
    return true;
#else
    BOOL w=FALSE; return IsWow64Process(GetCurrentProcess(),&w) && w;
#endif
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    if (!Is64()) { ErrorBox(L"Эта версия Prosto Happ предназначена для 64-битной Windows."); return 2; }
    std::wstring f=TempPath();
    if (f.empty() || !Download(HAPP_URL,f)) {
        if (!f.empty()) DeleteFileW(f.c_str());
        ErrorBox(L"Не удалось скачать последнюю версию Happ с GitHub.");
        return 3;
    }
    bool ok=RunInstaller(f); DeleteFileW(f.c_str());
    if (!ok) { KillHapp(); ErrorBox(L"Официальный установщик Happ завершился с ошибкой."); return 4; }

    KillHapp(); Configure(); ConfigureChrome();
    std::wstring happ=FindHapp();
    if (happ.empty()) {
        ErrorBox(L"Happ установлен, но Happ.exe не найден. Настройки уже применены.");
        return 5;
    }
    if ((INT_PTR)ShellExecuteW(nullptr,L"open",happ.c_str(),nullptr,nullptr,SW_SHOWNORMAL)<=32) {
        ErrorBox(L"Happ установлен и настроен, но не удалось его запустить.");
        return 6;
    }
    return 0;
}
