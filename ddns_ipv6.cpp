// 必须定义这个宏，防止 windows.h 包含旧版的 winsock.h 从而引发数百个冲突报错
#define WIN32_LEAN_AND_MEAN 

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>

// 核心修复：网络相关的头文件必须放在 windows.h 和 iphlpapi.h 之前
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <iphlpapi.h>

// 链接必要的 Windows 库
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// 强制隐藏控制台窗口 (适用于 MSVC 编译器)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

// ================= 配置区域 =================
// 全局配置变量
std::string API_TOKEN = "";
std::string DNS_NAME = "";
std::string ZONE_NAME = "";
int SLEEP_INTERVAL_SEC = 3600;

// 去除字符串首尾空白
std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// 加载配置文件
bool LoadConfig(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        // 如果文件不存在，创建一个默认模板
        std::ofstream outfile(filepath);
        if (outfile.is_open()) {
            outfile << "API_TOKEN=\n";
            outfile << "DNS_NAME=\n";
            outfile << "ZONE_NAME=\n";
            outfile << "SLEEP_INTERVAL_SEC=3600\n";
            outfile.close();
        }
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = Trim(line.substr(0, pos));
            std::string value = Trim(line.substr(pos + 1));
            
            if (key == "API_TOKEN") API_TOKEN = value;
            else if (key == "DNS_NAME") DNS_NAME = value;
            else if (key == "ZONE_NAME") ZONE_NAME = value;
            else if (key == "SLEEP_INTERVAL_SEC") {
                try {
                    SLEEP_INTERVAL_SEC = std::stoi(value);
                } catch (...) {
                    SLEEP_INTERVAL_SEC = 3600;
                }
            }
        }
    }
    
    // 简单检查必填项
    if (API_TOKEN.empty() || DNS_NAME.empty() || ZONE_NAME.empty()) {
        return false;
    }
    return true;
}
// ============================================

// 辅助函数：普通字符串转宽字符串
std::wstring ToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// 辅助函数：使用正则表达式提取简单的 JSON 值
std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    std::regex e("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    if (std::regex_search(json, m, e)) {
        return m[1].str();
    }
    return "";
}

// 获取本地 IPv6 地址 (排除 fe80 开头的本地链接地址)
std::string GetIPv6Address() {
    ULONG outBufLen = 15000;
    std::vector<BYTE> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)buffer.data();

    if (GetAdaptersAddresses(AF_INET6, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = (IP_ADAPTER_ADDRESSES*)buffer.data();
    }

    if (GetAdaptersAddresses(AF_INET6, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses != NULL; pCurrAddresses = pCurrAddresses->Next) {
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                sockaddr_in6* sockaddr = (sockaddr_in6*)pUnicast->Address.lpSockaddr;
                char ipStr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &(sockaddr->sin6_addr), ipStr, INET6_ADDRSTRLEN);
                std::string ip(ipStr);

                // 过滤掉 fe80 链路本地地址
                if (ip.find("fe80") != 0 && ip.find("::") != 0) {
                    return ip;
                }
            }
        }
    }
    return "";
}

// 封装的 HTTPS 请求函数
std::string SendHttpRequest(const std::string& method, const std::string& path, const std::string& body = "") {
    std::wstring wMethod = ToWString(method);
    std::wstring wPath = ToWString(path);

    HINTERNET hSession = WinHttpOpen(L"DDNS Client/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.cloudflare.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::wstring headers = L"Authorization: Bearer " + ToWString(API_TOKEN) + L"\r\nContent-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(), body.length(), body.length(), 0);

    std::string responseStr = "";
    if (bResults && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize == 0) break;
            char* pszOutBuffer = new char[dwSize + 1];
            ZeroMemory(pszOutBuffer, dwSize + 1);
            WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded);
            responseStr += pszOutBuffer;
            delete[] pszOutBuffer;
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return responseStr;
}

// 主干运行逻辑
void RunDDNS() {
    std::string ipv6 = GetIPv6Address();
    if (ipv6.empty()) return;

    // 1. 获取 Zone ID
    std::string zonePath = "/client/v4/zones?name=" + ZONE_NAME;
    std::string zoneRes = SendHttpRequest("GET", zonePath);
    std::string zoneID = ExtractJsonValue(zoneRes, "id");
    if (zoneID.empty()) return;

    // 2. 获取当前 DNS 记录
    std::string recordPath = "/client/v4/zones/" + zoneID + "/dns_records?type=AAAA&name=" + DNS_NAME;
    std::string recordRes = SendHttpRequest("GET", recordPath);
    std::string recordID = ExtractJsonValue(recordRes, "id");

    if (!recordID.empty()) {
        std::string currentIP = ExtractJsonValue(recordRes, "content");
        // 检查 IP 是否有变化
        if (currentIP != ipv6) {
            // 3. 更新 DNS 记录
            std::string updatePath = "/client/v4/zones/" + zoneID + "/dns_records/" + recordID;
            std::string body = "{\"type\":\"AAAA\",\"name\":\"" + DNS_NAME + "\",\"content\":\"" + ipv6 + "\",\"ttl\":1,\"proxied\":false}";
            SendHttpRequest("PUT", updatePath, body);
        }
    }
    else {
        // 4. 创建 DNS 记录 (如果不存在)
        std::string createPath = "/client/v4/zones/" + zoneID + "/dns_records";
        std::string body = "{\"type\":\"AAAA\",\"name\":\"" + DNS_NAME + "\",\"content\":\"" + ipv6 + "\",\"ttl\":1,\"proxied\":false}";
        SendHttpRequest("POST", createPath, body);
    }
}

int main() {
    // 防止程序启动时闪烁黑框
    FreeConsole();

    // 尝试加载配置
    if (!LoadConfig("config.ini")) {
        // 配置加载失败或文件刚创建，弹出提示并退出
        MessageBoxW(NULL, L"Please configure the 'config.ini' file first.\n请先在同目录下配置 'config.ini' 文件。", L"Configuration Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建互斥锁，防止程序多开
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Cloudflare_DDNS_IPv6_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0; // 如果已经有一个在运行，则静默退出
    }

    // 替代批处理的无限循环
    while (true) {
        RunDDNS();
        Sleep(SLEEP_INTERVAL_SEC * 1000); 
    }

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return 0;
}