#ifndef BINDINGS_H
#define BINDINGS_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "rtbot/Message.h"

std::string validate(std::string const& json_program);
std::string validateOperator(std::string const& type, std::string const& json_op);

std::string createProgram(std::string const& id, std::string const& json_program);
std::string deleteProgram(std::string const& id);
std::vector<std::optional<rtbot::Message<std::uint64_t, double>>> processMessage(
    std::string const& id, rtbot::Message<std::uint64_t, double> const& msg);

std::string processMessageDebug(std::string const& id, unsigned long time, double value);

#endif  // BINDINGS_H
