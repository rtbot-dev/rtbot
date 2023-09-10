#include "RtBotVisitor.h"

#include "compiler/antlr4/rtbotBaseVisitor.h"
#include "compiler/antlr4/rtbotParser.h"

using namespace rtbot_parser_internal;

std::any RtBotVisitor::visitStart(rtbotParser::StartContext *context) {}

std::any RtBotVisitor::visitOpExpr(rtbotParser::OpExprContext *context) {}

std::any RtBotVisitor::visitAtomExpr(rtbotParser::AtomExprContext *context) {}
