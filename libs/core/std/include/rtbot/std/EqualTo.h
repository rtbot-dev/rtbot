#ifndef EQUALTO_H
#define EQUALTO_H

#include "rtbot/FilterByValue.h"

namespace rtbot {

/**
 * @jsonschema
 * type: object
 * description: |
 *   Emits only the messages received with value equal to the number set. Messages that do not
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
struct EqualTo : public FilterByValue<T, V> {
  EqualTo() = default;

  EqualTo(string const &id, V value) : FilterByValue<T, V>(id, [=](V number) { return number == value; }) {
    this->value = value;
  }
  string typeName() const override { return "EqualTo"; }

  V getValue() const { return this->value; }

 private:
  V value;
};

}  // namespace rtbot

#endif  // EQUALTO_H