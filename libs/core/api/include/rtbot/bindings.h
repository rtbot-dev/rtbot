#ifndef BINDINGS_H
#define BINDINGS_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "rtbot/Message.h"

using namespace std;
using namespace rtbot;

string validate(string const& json_program);
string validateOperator(string const& type, string const& json_op);

string createProgram(string const& id, string const& json_program);
string deleteProgram(string const& id);

string addToMessageBuffer(const string& apId, const string& portId, Message<uint64_t, double> msg);
string processMessageBuffer(const string& apId);
string processMessageBufferDebug(const string& apId);

string getProgramEntryOperatorId(const string& apId);
string getProgramEntryPorts(const string& apId);
string getProgramOutputFilter(const string& apId);

string processMessageMap(string const& id, const map<string, vector<Message<uint64_t, double>>>& messagesMap);
string processMessageMapDebug(string const& id, const map<string, vector<Message<uint64_t, double>>>& messagesMap);

#endif  // BINDINGS_H
