#pragma once

#include <optional>
#include <string>
#include <vector>

namespace scwx
{
namespace util
{

/**
 * @brief Parse a list of tokens from a string
 *
 * This function will take an input string, and apply the delimiters vector in
 * order to tokenize the string. Each set of delimiters in the delimiters vector
 * will be used once. A set of delimiters will be used to match any character,
 * rather than a sequence of characters. Tokens are automatically trimmed of any
 * whitespace.
 *
 * @param [in] s Input string to tokenize
 * @param [in] delimiters A vector of delimiters to use for each token.
 * @param [in] pos Search begin position. Default is 0.
 *
 * @return Tokenized string
 */
std::vector<std::string> ParseTokens(const std::string&       s,
                                     std::vector<std::string> delimiters,
                                     std::size_t              pos = 0);

std::string ToString(const std::vector<std::string>& v);

template<typename T>
std::optional<T> TryParseUnsignedLong(const std::string& str);

} // namespace util
} // namespace scwx
