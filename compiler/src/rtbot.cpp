#include <iostream>

#include "CLI/CLI.hpp"
#include "RtBotVisitor.h"
#include "antlr4-runtime.h"
#include "compiler/antlr4/rtbotLexer.h"
#include "compiler/antlr4/rtbotParser.h"
#include "compiler/antlr4/rtbotVisitor.h"

using namespace std;
using namespace antlr4;
using namespace rtbot_parser_internal;

int main(int argc, const char* argv[]) {
  CLI::App app{"RtBot compiler. For more information visit https://rtbot.dev"};
  string filename = "";
  app.add_option("-f,--file", filename, "The file to compile")->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);
  cout << "Compiling " << filename << endl;

  ifstream stream;
  stream.open(filename);
  ANTLRInputStream input(stream);
  rtbotLexer lexer(&input);
  CommonTokenStream tokens(&lexer);

  rtbotParser parser(&tokens);
  rtbotParser::StartContext* tree = parser.start();
  RtBotVisitor visitor;

  return 0;
}
