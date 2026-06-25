#include "viewer_app.h"

#include "nvp/nvpsystem.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
  using namespace headless_training;

  const ViewerCliParseResult parsed = ParseViewerCommandLine(argc, argv);
  if(!parsed.ok)
  {
    std::cerr << "robot_training_viewer: " << parsed.error << "\n\n" << BuildViewerHelpText();
    return 2;
  }
  if(parsed.options.showHelp)
  {
    std::cout << BuildViewerHelpText();
    return 0;
  }

  try
  {
    NVPSystem system("robot_training_viewer");
    ViewerApp app(parsed.options);
    app.run();
  }
  catch(const std::exception& e)
  {
    std::cerr << "robot_training_viewer: " << e.what() << '\n';
    return 1;
  }

  return 0;
}
