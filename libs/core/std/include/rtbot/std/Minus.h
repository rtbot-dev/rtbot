#ifndef MINUS_H
#define MINUS_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Synchronizes two streams and emits the difference between the values for a given $t$.
 *   Synchronization mechanism inherited from `Join`.
 *   $$y(t_n)=x_1(t_n) - x_2(t_n)$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 * required: ["id"]
 */
template <class T, class V>
struct Minus : public BinaryJoin<T, V> {
  Minus() = default;
  Minus(string const &id) : BinaryJoin<T, V>(id, [](V a, V b) { return a - b; }) {}

  string typeName() const override { return "Minus"; }
};

}  // namespace rtbot

#endif  // MINUS_H