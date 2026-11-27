/**
 * @file cli_options.hpp
 * @brief Разбор параметров командной строки вида `--name value`.
 */
#pragma once

#include <map>
#include <string>
#include <vector>

namespace quant {

/// Параметры командной строки: имя без "--" → значение. Флаг `--help` хранится с пустым значением.
using CommandLineOptions = std::map<std::string, std::string>;

/**
 * @brief Разбирает аргументы `--name value ...`.
 * @param arguments Аргументы после имени команды.
 * @return Параметры.
 * @throws std::invalid_argument для аргумента без "--" или параметра без значения.
 */
CommandLineOptions ParseCommandLineOptions(const std::vector<std::string>& arguments);

/**
 * @brief Значение обязательного параметра.
 * @throws std::invalid_argument если параметр не задан.
 */
std::string GetRequiredOption(const CommandLineOptions& options, const std::string& name);

/**
 * @brief Значение параметра или @p default_value, если параметр не задан.
 */
std::string GetOptionOrDefault(const CommandLineOptions& options, const std::string& name,
                               const std::string& default_value);

/**
 * @brief Значение параметра как вещественное число.
 * @throws std::invalid_argument если значение не число.
 */
double GetDoubleOption(const CommandLineOptions& options, const std::string& name,
                       double default_value);

/**
 * @brief Значение параметра как целое число.
 * @throws std::invalid_argument если значение не целое число.
 */
int GetIntOption(const CommandLineOptions& options, const std::string& name, int default_value);

}  // namespace quant
