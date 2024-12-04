#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>

#include "args.h"
#include "data_loader.h"
#include "rtbot/bindings.h"
#include "ui_components.h"

using namespace rtbot_cli;
using namespace ftxui;

int main(int argc, char* argv[]) {
  auto args = CLIArguments::parse(argc, argv);
  auto screen = ScreenInteractive::Fullscreen();

  // Create title with scale information
  std::string title = "RTBot Interactive Mode";
  if (args.scale_t != 1.0) {
    title += " [t×" + std::to_string(args.scale_t) + "]";
  }
  if (args.scale_y != 1.0) {
    title += " [y×" + std::to_string(args.scale_y) + "]";
  }

  AppState state;
  state.args = args;
  state.programs = DataLoader::load_programs(args.programs_dir);
  state.csv_files = DataLoader::load_csv_files(args.csv_dir);

  auto container = UIComponents::create_main_layout(state);
  auto renderer = Renderer(container, [&] {
    return vbox({text(title) | bold | color(Color::Blue) | hcenter, separator(), container->Render() | flex});
  });
  screen.Loop(renderer);
  return 0;
}