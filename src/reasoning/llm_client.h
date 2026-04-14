#pragma once

#include <string>

struct LlmResult {
    std::string summary;
    std::string risk_level;
    std::string recommended_action;
    bool success{false};
    std::string error_message;
};

class LlmClient {
public:
    LlmClient();

    LlmResult reasonOverScene(const std::string& scene_state_json) const;

private:
    static std::wstring utf8ToWide(const std::string& input);
    static std::string wideToUtf8(const std::wstring& input);

    static std::string extractJsonStringValue(const std::string& json,
                                              const std::string& key);
    static std::string unescapeJsonString(const std::string& input);

private:
    std::wstring host_;
    int port_{8000};
    std::wstring path_;
};