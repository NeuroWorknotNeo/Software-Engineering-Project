/**
 * @file cli_options.cpp
 * @brief Реализация разбора параметров командной строки.
 */
#include "quant/io/cli_options.hpp"

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace quant {

namespace {

constexpr std::string_view kOptionPrefix = "--";

template <typename Number>
Number ParseNumberOption(const std::string& text, const std::string& name) {
    Number value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc() || end != text.data() + text.size()) {
        throw std::invalid_argument("--" + name + " must be a number, got '" + text + "'");
    }
    return value;
}

}  // namespace

CommandLineOptions ParseCommandLineOptions(const std::vector<std::string>& arguments) {
    CommandLineOptions options;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& key = arguments[index];
        if (!key.starts_with(kOptionPrefix)) {
            throw std::invalid_argument("unexpected argument '" + key + "'");
        }
        const std::string name = key.substr(kOptionPrefix.size());
        if (name == "help") {
            options[name] = "";
            continue;
        }
        if (index + 1 >= arguments.size()) {
            throw std::invalid_argument("missing value for " + key);
        }
        ++index;
        options[name] = arguments[index];
    }
    return options;
}

std::string GetRequiredOption(const CommandLineOptions& options, const std::string& name) {
    const auto found = options.find(name);
    if (found == options.end()) {
        throw std::invalid_argument("--" + name + " is required");
    }
    return found->second;
}

std::string GetOptionOrDefault(const CommandLineOptions& options, const std::string& name,
                               const std::string& default_value) {
    const auto found = options.find(name);
    return found == options.end() ? default_value : found->second;
}

double GetDoubleOption(const CommandLineOptions& options, const std::string& name,
                       double default_value) {
    const auto found = options.find(name);
    return found == options.end() ? default_value : ParseNumberOption<double>(found->second, name);
}

int GetIntOption(const CommandLineOptions& options, const std::string& name, int default_value) {
    const auto found = options.find(name);
    return found == options.end() ? default_value : ParseNumberOption<int>(found->second, name);
}

}  // namespace quant
