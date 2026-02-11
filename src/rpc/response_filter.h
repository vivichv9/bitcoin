// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_RESPONSE_FILTER_H
#define BITCOIN_RPC_RESPONSE_FILTER_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class ArgsManager;
class UniValue;

namespace rpc {

struct StripFieldPathSegment {
    std::string key;
    bool select_all_array_items{false};
    std::optional<size_t> index{};
};

struct StripFieldRule {
    std::string method;
    std::string path;
    std::vector<StripFieldPathSegment> compiled_path;
};

class StripFieldConfig
{
public:
    void AddRule(StripFieldRule rule);
    void AddInvalidRule(std::string raw_rule);

    [[nodiscard]] bool Empty() const { return m_rule_count == 0; }
    [[nodiscard]] size_t RuleCount() const { return m_rule_count; }
    [[nodiscard]] size_t GlobalRuleCount() const { return m_global_rules.size(); }
    [[nodiscard]] size_t MethodRuleCount() const { return m_rule_count - m_global_rules.size(); }

    [[nodiscard]] const std::vector<StripFieldRule>& GetGlobalRules() const { return m_global_rules; }
    [[nodiscard]] const std::vector<StripFieldRule>* GetRulesForMethod(std::string_view method) const;
    [[nodiscard]] const std::vector<std::string>& InvalidRules() const { return m_invalid_rules; }

private:
    std::unordered_map<std::string, std::vector<StripFieldRule>> m_method_rules;
    std::vector<StripFieldRule> m_global_rules;
    std::vector<std::string> m_invalid_rules;
    size_t m_rule_count{0};
};

StripFieldConfig LoadStripFieldConfig(const ArgsManager& argsman);
bool ValidateStripFieldConfig(const ArgsManager& argsman, std::string& error);
void ApplyStripFieldRules(const StripFieldConfig& config, std::string_view method, UniValue& result);

} // namespace rpc

#endif // BITCOIN_RPC_RESPONSE_FILTER_H
