#ifndef COMPOSITE_CONNECTION_H
#define COMPOSITE_CONNECTION_H

#include <cstddef>
#include <string>

namespace rtbot {

// Descriptive record of an internal wiring inside a composite operator
// (Pipeline, TriggerSet, ...). Stored only so the composite can be re-emitted
// back to JSON when serialized; runtime routing happens through the live
// Operator::Connection list on each child.
struct CompositeConnection {
  std::string from_id;
  std::string to_id;
  std::size_t from_port{0};
  std::size_t to_port{0};
};

}  // namespace rtbot

#endif  // COMPOSITE_CONNECTION_H
