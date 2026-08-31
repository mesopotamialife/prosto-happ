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
#include <cwchar>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

static const wchar_t* HAPP_URL =
    L"https://github.com/Happ-proxy/happ-desktop/releases/latest/download/setup-Happ.x64.exe";

static void ErrorBox(const std::wstring& text)
{
    MessageBoxW(nullptr, text.c_str(), L"Prosto Happ", MB_OK | MB_ICONERROR);
}

static bool FileExists(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring GetTempInstallerPath()
{
    wchar_t dir[MAX_PATH] = {};
    wchar_t temp[MAX_PATH] = {};

    if (!GetTempPathW(MAX_PATH, dir))
        return L"";

    // GetTempFileName creates a file with .tmp. We remove it and use the
    // same unique name with .exe so Windows/Shell treats the downloaded file
    // as an executable.
    if (!GetTempFileNameW(dir, L"PHH", 0, temp))
        return L"";

    DeleteFileW(temp);

    std::wstring path = temp;
    size_t dot = path.rfind(L'.');
    if (dot != std::wstring::npos)
        path.erase(dot);

    path += L".exe";
    return path;
}

static bool DownloadHttps(const std::wstring& url,
                          const std::wstring& destination)
{
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);

    wchar_t host[512] = {};
    wchar_t path[4096] = {};
    wchar_t extra[4096] = {};

    uc.lpszHostName = host;
    uc.dwHostNameLength = static_cast<DWORD>(_countof(host));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = static_cast<DWORD>(_countof(path));
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = static_cast<DWORD>(_countof(extra));

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        return false;

    HINTERNET session = WinHttpOpen(
        L"Prosto-Happ/1.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!session)
        return false;

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(
        session,
        WINHTTP_OPTION_SECURE_PROTOCOLS,
        &protocols,
        sizeof(protocols));

    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect)
    {
        WinHttpCloseHandle(session);
        return false;
    }

    std::wstring object = std::wstring(path) + extra;

    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"GET",
        object.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!request)
    {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    bool ok = false;

    if (WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0) &&
        WinHttpReceiveResponse(request, nullptr))
    {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);

        if (WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX) &&
            status >= 200 && status < 300)
        {
            HANDLE file = CreateFileW(
                destination.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (file != INVALID_HANDLE_VALUE)
            {
                ok = true;

                BYTE buffer[64 * 1024];
                DWORD bytesRead = 0;
                DWORD bytesWritten = 0;

                for (;;)
                {
                    if (!WinHttpReadData(
                            request,
                            buffer,
                            sizeof(buffer),
                            &bytesRead))
                    {
                        ok = false;
                        break;
                    }

                    if (bytesRead == 0)
                        break;

                    if (!WriteFile(
                            file,
                            buffer,
                            bytesRead,
                            &bytesWritten,
                            nullptr) ||
                        bytesWritten != bytesRead)
                    {
                        ok = false;
                        break;
                    }
                }

                CloseHandle(file);
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    return ok;
}

static void KillHapp()
{
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    wchar_t command[] = L"taskkill.exe /F /IM Happ.exe /T";

    if (CreateProcessW(
            nullptr,
            command,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi))
    {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    Sleep(500);
}

static DWORD RunOfficialInstaller(const std::wstring& installer)
{
    // The bootstrapper itself is requireAdministrator, so the official
    // installer inherits an elevated token. No second UAC prompt is needed.
    const std::wstring args =
        L"/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS";

    std::wstring commandLine = L"\"" + installer + L"\" " + args;

    std::vector<wchar_t> mutableCommand(
        commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    BOOL created = CreateProcessW(
        installer.c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!created)
        return GetLastError();

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode;
}

static void SetString(HKEY root,
                      const wchar_t* subkey,
                      const wchar_t* name,
                      const wchar_t* value)
{
    HKEY key = nullptr;

    if (RegCreateKeyExW(
            root,
            subkey,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(
            key,
            name,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value),
            static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));

        RegCloseKey(key);
    }
}

static void SetDword(HKEY root,
                     const wchar_t* subkey,
                     const wchar_t* name,
                     DWORD value)
{
    HKEY key = nullptr;

    if (RegCreateKeyExW(
            root,
            subkey,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(
            key,
            name,
            0,
            REG_DWORD,
            reinterpret_cast<const BYTE*>(&value),
            sizeof(value));

        RegCloseKey(key);
    }
}

static void ConfigureHapp()
{
    const wchar_t* base =
        L"Software\\Happ\\OrganizationDefaults\\Preferences";

    SetString(HKEY_CURRENT_USER, base, L"language", L"ru");
    SetString(HKEY_CURRENT_USER, base, L"mode", L"Light");
    SetString(HKEY_CURRENT_USER, base, L"fontScale", L"1");

    const wchar_t* advanced =
        L"Software\\Happ\\OrganizationDefaults\\Preferences\\AdvancedSettings";

    SetString(HKEY_CURRENT_USER, advanced, L"autoStart", L"true");
    SetString(HKEY_CURRENT_USER, advanced, L"tun", L"false");
    SetDword(HKEY_CURRENT_USER, advanced, L"tunMode", 0);
    SetString(HKEY_CURRENT_USER, advanced, L"tunProvider", L"sing-box");

    const wchar_t* ping =
        L"Software\\Happ\\OrganizationDefaults\\Preferences\\PingSettings";

    SetString(
        HKEY_CURRENT_USER,
        ping,
        L"pingTestUrl",
        L"http://www.gstatic.com/generate_204");

    SetDword(HKEY_CURRENT_USER, ping, L"pingType", 1);
    SetString(HKEY_CURRENT_USER, ping, L"proxyPingMode", L"double");

    const wchar_t* subscriptions =
        L"Software\\Happ\\OrganizationDefaults\\Preferences\\Subscriptions";

    SetString(HKEY_CURRENT_USER, subscriptions, L"subsSendHWID", L"true");
    SetString(HKEY_CURRENT_USER, subscriptions, L"subsAlternativeHWID", L"true");
    SetString(HKEY_CURRENT_USER, subscriptions, L"subsUpdateOnOpen", L"true");
    SetString(HKEY_CURRENT_USER, subscriptions, L"subsAutoUpdate", L"true");
    SetString(HKEY_CURRENT_USER, subscriptions, L"subsReadOnlyHWID", L"true");

    const wchar_t* routing =
        L"Software\\Happ\\OrganizationDefaults\\Preferences\\TunnelSettings\\Routing";

    SetString(
        HKEY_CURRENT_USER,
        routing,
        L"geoFilesUserAgent",
        L"firefox-win");
}

static void ConfigureChrome()
{
    SetString(
        HKEY_CURRENT_USER,
        L"Software\\Policies\\Google\\Chrome",
        L"ProxySettings",
        L"{\"ProxyMode\":\"system\"}");
}

static std::wstring FindHappExe()
{
    std::vector<std::wstring> candidates = {
        L"C:\\Program Files\\Happ\\Happ.exe",
        L"C:\\Program Files\\FlyFrogLLC\\Happ\\Happ.exe",
        L"C:\\Program Files (x86)\\Happ\\Happ.exe",
        L"C:\\Program Files (x86)\\FlyFrogLLC\\Happ\\Happ.exe"
    };

    wchar_t localAppData[MAX_PATH] = {};

    if (SUCCEEDED(SHGetFolderPathW(
            nullptr,
            CSIDL_LOCAL_APPDATA,
            nullptr,
            SHGFP_TYPE_CURRENT,
            localAppData)))
    {
        candidates.push_back(
            std::wstring(localAppData) + L"\\Happ\\Happ.exe");

        candidates.push_back(
            std::wstring(localAppData) + L"\\Programs\\Happ\\Happ.exe");
    }

    for (const auto& path : candidates)
    {
        if (FileExists(path))
            return path;
    }

    return L"";
}

static bool Is64BitWindows()
{
#if defined(_WIN64)
    return true;
#else
    BOOL wow64 = FALSE;
    return IsWow64Process(GetCurrentProcess(), &wow64) && wow64;
#endif
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    if (!Is64BitWindows())
    {
        ErrorBox(
            L"Эта версия Prosto Happ предназначена для 64-битной Windows.");
        return 2;
    }

    std::wstring installer = GetTempInstallerPath();

    if (installer.empty())
    {
        ErrorBox(L"Не удалось создать временный файл.");
        return 3;
    }

    if (!DownloadHttps(HAPP_URL, installer))
    {
        DeleteFileW(installer.c_str());

        ErrorBox(
            L"Не удалось скачать последнюю версию Happ с GitHub.\n\n"
            L"Проверьте интернет-соединение и попробуйте ещё раз.");

        return 4;
    }

    DWORD installerResult = RunOfficialInstaller(installer);
    DeleteFileW(installer.c_str());

    if (installerResult != 0)
    {
        KillHapp();

        std::wstring message =
            L"Официальный установщик Happ завершился с ошибкой.\n\n"
            L"Код возврата: " + std::to_wstring(installerResult);

        ErrorBox(message);
        return 5;
    }

    KillHapp();

    ConfigureHapp();
    ConfigureChrome();

    std::wstring happExe = FindHappExe();

    if (happExe.empty())
    {
        ErrorBox(
            L"Happ был установлен, но файл Happ.exe не найден.\n\n"
            L"Настройки уже применены. Запустите Happ из меню Пуск.");

        return 6;
    }

    if ((INT_PTR)ShellExecuteW(
            nullptr,
            L"open",
            happExe.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL) <= 32)
    {
        ErrorBox(
            L"Happ установлен и настроен, но не удалось его запустить.");

        return 7;
    }

    return 0;
}
