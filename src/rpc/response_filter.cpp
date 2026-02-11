// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/response_filter.h>

#include <common/args.h>
#include <util/check.h>
#include <util/string.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include <univalue.h>

using util::TrimString;
using util::TrimStringView;

namespace rpc {

static bool IsMethodValid(std::string_view method)
{
    if (method == "*") return true;
    return !method.empty() && std::none_of(method.begin(), method.end(), [](unsigned char c) {
        return std::isspace(c) || c == ':';
    });
}

static bool ParsePathSegment(const std::string_view token, StripFieldPathSegment& segment_out, std::string& error)
{
    const auto bracket_open{token.find('[')};
    if (bracket_open == std::string_view::npos) {
        if (token.empty()) {
            error = "Path contains an empty segment";
            return false;
        }
        if (token.find(']') != std::string_view::npos) {
            error = "Path contains unexpected ']'";
            return false;
        }
        segment_out.key = std::string{token};
        return true;
    }

    const auto bracket_close{token.find(']', bracket_open + 1)};
    if (bracket_close == std::string_view::npos || bracket_close != token.size() - 1) {
        error = "Array selector must be the final part of a path segment";
        return false;
    }
    if (token.find('[', bracket_open + 1) != std::string_view::npos) {
        error = "Multiple array selectors in one segment are not supported";
        return false;
    }

    const std::string_view key{token.substr(0, bracket_open)};
    if (key.empty()) {
        error = "Array selector requires a field name before '['";
        return false;
    }
    segment_out.key = std::string{key};

    const std::string_view selector{token.substr(bracket_open + 1, bracket_close - bracket_open - 1)};
    if (selector.empty()) {
        segment_out.select_all_array_items = true;
        return true;
    }
    if (!std::all_of(selector.begin(), selector.end(), [](unsigned char c) { return std::isdigit(c); })) {
        error = "Array index must contain decimal digits only";
        return false;
    }

    try {
        const auto idx{std::stoull(std::string{selector})};
        if (idx > std::numeric_limits<size_t>::max()) {
            error = "Array index is out of range";
            return false;
        }
        segment_out.index = static_cast<size_t>(idx);
    } catch (const std::exception&) {
        error = "Array index is out of range";
        return false;
    }
    return true;
}

static bool ParsePath(const std::string_view path, std::vector<StripFieldPathSegment>& compiled_path, std::string& error)
{
    if (path.empty()) {
        error = "Path is empty";
        return false;
    }
    if (path.front() == '.' || path.back() == '.') {
        error = "Path cannot start or end with '.'";
        return false;
    }

    size_t offset{0};
    while (offset < path.size()) {
        const size_t dot{path.find('.', offset)};
        const std::string_view token{
            dot == std::string_view::npos ? path.substr(offset) : path.substr(offset, dot - offset)
        };
        if (token.empty()) {
            error = "Path contains an empty segment";
            return false;
        }

        StripFieldPathSegment segment;
        if (!ParsePathSegment(token, segment, error)) return false;
        compiled_path.push_back(std::move(segment));

        if (dot == std::string_view::npos) break;
        offset = dot + 1;
    }
    return !compiled_path.empty();
}

static bool ParseRule(const std::string& raw_entry, StripFieldRule& rule_out, std::string& error)
{
    const std::string_view entry{TrimStringView(raw_entry)};
    const size_t delim{entry.find(':')};
    if (delim == std::string_view::npos) {
        error = "Expected '<method>:<path>'";
        return false;
    }

    rule_out.method = TrimString(entry.substr(0, delim));
    rule_out.path = TrimString(entry.substr(delim + 1));
    if (!IsMethodValid(rule_out.method)) {
        error = "Method must be '*' or a non-empty method name without whitespace";
        return false;
    }
    if (!ParsePath(rule_out.path, rule_out.compiled_path, error)) return false;
    return true;
}

void StripFieldConfig::AddRule(StripFieldRule rule)
{
    if (rule.method == "*") {
        m_global_rules.push_back(std::move(rule));
    } else {
        m_method_rules[rule.method].push_back(std::move(rule));
    }
    ++m_rule_count;
}

void StripFieldConfig::AddInvalidRule(std::string raw_rule)
{
    m_invalid_rules.push_back(std::move(raw_rule));
}

const std::vector<StripFieldRule>* StripFieldConfig::GetRulesForMethod(std::string_view method) const
{
    const auto it = m_method_rules.find(std::string{method});
    if (it == m_method_rules.end()) return nullptr;
    return &it->second;
}

StripFieldConfig LoadStripFieldConfig(const ArgsManager& argsman)
{
    StripFieldConfig config;
    const auto rules = argsman.GetArgs("-rpcstripfield");
    for (const std::string& raw_entry : rules) {
        StripFieldRule rule;
        std::string error;
        if (!ParseRule(raw_entry, rule, error)) {
            config.AddInvalidRule("Invalid -rpcstripfield value '" + raw_entry + "': " + error);
            continue;
        }
        config.AddRule(std::move(rule));
    }
    return config;
}

bool ValidateStripFieldConfig(const ArgsManager& argsman, std::string& error)
{
    const auto rules = argsman.GetArgs("-rpcstripfield");
    for (const std::string& raw_entry : rules) {
        StripFieldRule rule;
        if (!ParseRule(raw_entry, rule, error)) {
            error = "Invalid -rpcstripfield value '" + raw_entry + "': " + error;
            return false;
        }
    }
    return true;
}

static UniValue ApplyRuleAt(const UniValue& node, const std::vector<StripFieldPathSegment>& path, size_t path_index);

static UniValue ApplyRuleToArray(const UniValue& array_node, const std::vector<StripFieldPathSegment>& path, size_t path_index)
{
    const auto& segment = path[path_index];
    const auto& values = array_node.getValues();

    UniValue out(UniValue::VARR);
    out.reserve(values.size());

    if (segment.select_all_array_items) {
        if (path_index + 1 == path.size()) {
            return out; // remove all selected elements
        }
        for (const UniValue& value : values) {
            out.push_back(ApplyRuleAt(value, path, path_index + 1));
        }
        return out;
    }

    CHECK_NONFATAL(segment.index.has_value());
    for (size_t i{0}; i < values.size(); ++i) {
        if (i != *segment.index) {
            out.push_back(values[i]);
            continue;
        }
        if (path_index + 1 == path.size()) {
            continue; // remove selected element
        }
        out.push_back(ApplyRuleAt(values[i], path, path_index + 1));
    }
    return out;
}

static UniValue ApplyRuleAt(const UniValue& node, const std::vector<StripFieldPathSegment>& path, size_t path_index)
{
    if (!node.isObject()) {
        return node;
    }

    const auto& segment = path[path_index];
    const auto& keys = node.getKeys();
    const auto& values = node.getValues();

    UniValue out(UniValue::VOBJ);
    out.reserve(values.size());

    for (size_t i{0}; i < keys.size(); ++i) {
        const std::string& key = keys[i];
        const UniValue& value = values[i];
        if (key != segment.key) {
            out.pushKVEnd(key, value);
            continue;
        }

        if (!segment.select_all_array_items && !segment.index.has_value()) {
            if (path_index + 1 == path.size()) {
                continue; // remove field
            }
            out.pushKVEnd(key, ApplyRuleAt(value, path, path_index + 1));
            continue;
        }

        if (!value.isArray()) {
            out.pushKVEnd(key, value);
            continue;
        }
        out.pushKVEnd(key, ApplyRuleToArray(value, path, path_index));
    }

    return out;
}

static void ApplyRule(const StripFieldRule& rule, UniValue& result)
{
    result = ApplyRuleAt(result, rule.compiled_path, 0);
}

void ApplyStripFieldRules(const StripFieldConfig& config, std::string_view method, UniValue& result)
{
    if (config.Empty()) return;

    for (const auto& rule : config.GetGlobalRules()) {
        ApplyRule(rule, result);
    }

    const auto method_rules{config.GetRulesForMethod(method)};
    if (!method_rules) return;
    for (const auto& rule : *method_rules) {
        ApplyRule(rule, result);
    }
}

} // namespace rpc
