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

string createProgram(string const& programId, string const& json_program);
string deleteProgram(string const& programId);

// TODO: wasm support for 64 bit looks uncertain, hence we use only `unsigned long` for the api
string addToMessageBuffer(const string& programId, const string& portId, unsigned long time, double value);
string processMessageBuffer(const string& programId);
string processMessageBufferDebug(const string& programId);

string getProgramEntryOperatorId(const string& programId);
string getProgramEntryPorts(const string& programId);
string getProgramOutputFilter(const string& programId);

string processMessageMap(string const& programId, const map<string, vector<Message<uint64_t, double>>>& messagesMap);
string processMessageMapDebug(string const& programId,
                              const map<string, vector<Message<uint64_t, double>>>& messagesMap);

string processBatch(string const& programId, vector<uint64_t> times, vector<double> values,
                    vector<string> const& ports);
string processBatchDebug(string const& programId, vector<uint64_t> times, vector<double> values,
                         vector<string> const& ports);

#endif  // BINDINGS_H
