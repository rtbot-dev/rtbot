#ifndef MULTIPLICATION_H
#define MULTIPLICATION_H

#include "rtbot/BinaryJoin.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   Synchronizes two streams and emits the multiplication between the values for a given $t$.
 *   Synchronization mechanism inherited from `Join`.
 *   $$y(t_n)=x_1(t_n) * x_2(t_n)$$
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 * required: ["id"]
 */
template <class T, class V>
struct Multiplication : public BinaryJoin<T, V> {
  Multiplication() = default;
  Multiplication(string const &id) : BinaryJoin<T, V>(id, [](V a, V b) { return a * b; }) {}

  string typeName() const override { return "Multiplication"; }
};

}  // namespace rtbot

#endif  // MULTIPLICATION_H