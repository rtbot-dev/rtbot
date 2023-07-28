#ifndef LESSTHAN_H
#define LESSTHAN_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

/**
 * @jsonschema
 * type: object
 * description: |
 *   Emits only the messages received with value less than the number set. Messages that do not
 *   comply with the condition are ignored.
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   value:
 *     type: integer
 *     description: The reference value
 * required: ["id", "value"]
 */
template <class T, class V>
struct LessThan : public FilterByValue<T, V> {
  LessThan() = default;
  V x;
  LessThan(string const &id, V value) : x(value), FilterByValue<T, V>(id, [=](V number) { return number < value; }) {}
  string typeName() const override { return "LessThan"; }
};

}  // namespace rtbot

#endif  // LESSTHAN_H
