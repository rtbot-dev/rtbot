#include "rtbot/bindings.h"

#include <nlohmann/json.hpp>

#include "rtbot/FactoryOp.h"

rtbot::FactoryOp factory;

using json = nlohmann::json;

namespace rtbot {

template <class T, class V>
void to_json(json& j, const Message<T,V>& p) {
  j = json{{"time", p.time}, {"value", p.value}};
}

template <class T, class V>
void from_json(const json& j, Message<T,V>& p) {
  j.at("time").get_to(p.time);
  j.at("value").get_to(p.value);
}

}  // namespace rtbot

std::string createPipeline(std::string const& id, std::string const& json_program) {
  return factory.createPipeline(id, json_program);
}

std::string deletePipeline(std::string const& id) { return factory.deletePipeline(id); }

std::vector<std::optional<rtbot::Message<std::uint64_t,double>>> receiveMessageInPipeline(const std::string& id,
                                                                            rtbot::Message<std::uint64_t,double> const& msg) {
  return factory.receiveMessageInPipeline(id, msg);
}

std::string receiveMessageInPipelineDebug(std::string const& id, unsigned long time, double value) {
  rtbot::Message msg = rtbot::Message<std::uint64_t,double>(time, value);

  auto result = factory.receiveMessageInPipelineDebug(id, msg);
  return nlohmann::json(result).dump();
}
