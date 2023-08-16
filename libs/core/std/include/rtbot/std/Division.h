#ifndef DIVISION_H
#define DIVISION_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Synchronizes two streams and computes its division. Synchronization mechanism inherited from `Join`.
 *   $$y(t_n)=x_1(t_n) / x_2(t_n)$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 * required: ["id"]
 */
template <class T, class V>
struct Division : public BinaryJoin<T, V> {
  Division() = default;
  Division(string const &id) : BinaryJoin<T, V>(id, [](V a, V b) { return a / b; }) {}

  string typeName() const override { return "Division"; }
};

}  // namespace rtbot

#endif  // DIVIDE_H