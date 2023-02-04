#ifndef BINDINGS_H
#define BINDINGS_H

#include "rtbot/Message.h"
#include <string>
#include <vector>
#include <optional>

std::string createPipeline(std::string const& id, std::string const& json_program);
std::string deletePipeline(std::string const&  id);
std::vector<std::optional<rtbot::Message<>>> receiveMessageInPipeline(std::string const& id, rtbot::Message<> const& msg);


#endif // BINDINGS_H
