#ifndef BINDINGS_H
#define BINDINGS_H

#include <optional>
#include <string>
#include <vector>

#include "rtbot/Message.h"

std::string createPipeline(std::string const& id, std::string const& json_program);
std::string deletePipeline(std::string const& id);
std::vector<std::optional<rtbot::Message<>>> receiveMessageInPipeline(std::string const& id,
                                                                      rtbot::Message<> const& msg);

std::string receiveMessageInPipelineDebug(std::string const& id, unsigned long time, std::vector<double> value);

#endif  // BINDINGS_H
