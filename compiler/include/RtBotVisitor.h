#include "compiler/antlr4/rtbotBaseVisitor.h"
#include "compiler/antlr4/rtbotParser.h"

using namespace rtbot_parser_internal;
class RtBotVisitor : rtbotBaseVisitor {
 public:
  std::any visitStart(rtbotParser::StartContext *context);

  std::any visitOpExpr(rtbotParser::OpExprContext *context);

  std::any visitAtomExpr(rtbotParser::AtomExprContext *context);
};
