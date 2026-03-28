#pragma once

#include <span>
#include <string>
#include <vector>

namespace scwx::qt::main::ProgramOptions
{

struct Options
{
   bool                     showHelp_ {false};
   bool                     enableConsole_ {false};
   bool                     portableMode_ {false};
   std::string              settingsDirectory_ {};
   std::vector<std::string> unrecognizedArgs_ {};
};

/**
 * @brief Retrieves the parsed command line options.
 * @return A constant reference to the ProgramOptions struct.
 * @note This function should only be called after ParseArguments() has been
 * called to ensure that the command line options have been parsed and stored.
 */
[[nodiscard]] const Options& GetOptions();

/**
 * @brief Parses command line arguments and stores them in an Options struct.
 */
void ParseArguments(std::span<const char* const> args);

/**
 * @brief Handles the parsed command line arguments, performing any necessary
 * actions based on the options provided.
 *
 * @param exit A reference to a boolean that will be set to true if the program
 * should exit after handling the arguments (e.g., if the help option was
 * provided).
 *
 * @note This function should only be called after ParseArguments() has been
 * called to ensure that the command line options have been parsed and stored,
 * and after any logging has been initialized, so that it can log any relevant
 * information or warnings about the command line arguments.
 */
void HandleArguments(bool& exit);

/**
 * @brief Resets the parsed command line options to their default values.
 * @note Primarily intended for testing purposes.
 */
void Reset();

} // namespace scwx::qt::main::ProgramOptions
