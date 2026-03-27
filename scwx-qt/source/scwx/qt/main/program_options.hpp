#pragma once

#include <span>
#include <string>
#include <vector>

namespace scwx::qt::main
{

struct ProgramOptions
{
   bool                     showHelp_ {false};
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
[[nodiscard]] const ProgramOptions& GetProgramOptions();

/**
 * @brief Parses command line arguments and stores them in a ProgramOptions
 * struct.
 */
void ParseArguments(std::span<const char* const> args);

/**
 * @brief Handles the parsed command line arguments, performing any necessary
 * actions based on the options provided.
 *
 * @note This function should only be called after ParseArguments() has been
 * called to ensure that the command line options have been parsed and stored,
 * and after any logging has been initialized, so that it can log any relevant
 * information or warnings about the command line arguments.
 */
void HandleArguments();

} // namespace scwx::qt::main
