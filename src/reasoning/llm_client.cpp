#include "reasoning/llm_client.h"

#include <windows.h>
#include <winhttp.h>

#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

LlmClient::LlmClient()
    : host_(L"127.0.0.1"),
      port_(8000),
      path_(L"/reason") {}

std::wstring LlmClient::utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return L"";
    }

    const int size_needed = MultiByteToWideChar(
        CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);

    std::wstring result(size_needed, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), result.data(), size_needed);

    return result;
}

std::string LlmClient::wideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return "";
    }

    const int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);

    std::string result(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), result.data(), size_needed, nullptr, nullptr);

    return result;
}

std::string LlmClient::unescapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            const char next = input[i + 1];
            switch (next) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                default:
                    output.push_back(next);
                    break;
            }
            ++i;
        } else {
            output.push_back(input[i]);
        }
    }

    return output;
}

std::string LlmClient::extractJsonStringValue(const std::string& json,
                                              const std::string& key) {
    const std::regex pattern(
        "\"" + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");

    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1) {
        return unescapeJsonString(match[1].str());
    }

    return "";
}

LlmResult LlmClient::reasonOverScene(const std::string& scene_state_json) const {
    LlmResult result;

    const std::string request_body =
        std::string("{\"scene_state\":") + scene_state_json + "}";

    HINTERNET session = WinHttpOpen(
        L"Sentinel/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!session) {
        result.error_message = "WinHttpOpen failed.";
        return result;
    }

    HINTERNET connection = WinHttpConnect(
        session,
        host_.c_str(),
        static_cast<INTERNET_PORT>(port_),
        0);

    if (!connection) {
        WinHttpCloseHandle(session);
        result.error_message = "WinHttpConnect failed.";
        return result;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"POST",
        path_.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);

    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.error_message = "WinHttpOpenRequest failed.";
        return result;
    }

    const wchar_t* headers = L"Content-Type: application/json\r\n";

    const BOOL send_ok = WinHttpSendRequest(
        request,
        headers,
        -1L,
        const_cast<char*>(request_body.data()),
        static_cast<DWORD>(request_body.size()),
        static_cast<DWORD>(request_body.size()),
        0);

    if (!send_ok) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.error_message = "WinHttpSendRequest failed.";
        return result;
    }

    const BOOL receive_ok = WinHttpReceiveResponse(request, nullptr);
    if (!receive_ok) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        result.error_message = "WinHttpReceiveResponse failed.";
        return result;
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_code_size,
        WINHTTP_NO_HEADER_INDEX);

    std::string response_body;
    do {
        DWORD bytes_available = 0;
        if (!WinHttpQueryDataAvailable(request, &bytes_available)) {
            result.error_message = "WinHttpQueryDataAvailable failed.";
            break;
        }

        if (bytes_available == 0) {
            break;
        }

        std::string buffer(bytes_available, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request, buffer.data(), bytes_available, &bytes_read)) {
            result.error_message = "WinHttpReadData failed.";
            break;
        }

        buffer.resize(bytes_read);
        response_body += buffer;
    } while (true);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (!result.error_message.empty()) {
        return result;
    }

    if (status_code != 200) {
        std::ostringstream oss;
        oss << "LLM service returned HTTP " << status_code
            << " with body: " << response_body;
        result.error_message = oss.str();
        return result;
    }

    result.summary = extractJsonStringValue(response_body, "summary");
    result.risk_level = extractJsonStringValue(response_body, "risk_level");
    result.recommended_action = extractJsonStringValue(response_body, "recommended_action");

    if (result.summary.empty() || result.risk_level.empty() || result.recommended_action.empty()) {
        result.error_message = "Failed to parse LLM response: " + response_body;
        return result;
    }

    result.success = true;
    return result;
}