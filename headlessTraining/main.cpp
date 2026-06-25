#include "cli.h"
#include "training_runner.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
  using namespace headless_training;

  const CliParseResult parsed = ParseCommandLine(argc, argv);
  if(!parsed.ok)
  {
    std::cerr << "robot_training_headless: " << parsed.error << "\n\n" << BuildHelpText();
    return 2;
  }
  if(parsed.options.showHelp)
  {
    std::cout << BuildHelpText();
    return 0;
  }

  try
  {
    RunTraining(parsed.options);
  }
  catch(const std::exception& e)
  {
    std::cerr << "robot_training_headless: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
