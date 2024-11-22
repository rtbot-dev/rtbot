#ifndef FUNCTION_H
#define FUNCTION_H

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rtbot/Operator.h"

namespace rtbot {

template <class T, class V>
struct Function : public Operator<T, V> {
  enum class InterpolationType { LINEAR, HERMITE };

  Function() = default;

  Function(string const& id, vector<pair<V, V>> points, string type = "linear") : Operator<T, V>(id) {
    if (points.size() < 2) {
      throw runtime_error(typeName() + ": at least 2 points are required for interpolation");
    }

    sort(points.begin(), points.end());
    this->points = points;

    if (type == "linear") {
      this->type = InterpolationType::LINEAR;
    } else if (type == "hermite") {
      this->type = InterpolationType::HERMITE;
    } else {
      throw runtime_error(typeName() + ": invalid interpolation type. Use 'linear' or 'hermite'");
    }

    this->addDataInput("i1", 1);
    this->addOutput("o1");
  }

  virtual Bytes collect() override {
    Bytes bytes = Operator<T, V>::collect();

    // Serialize points
    size_t pointsSize = points.size();
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&pointsSize),
                 reinterpret_cast<const unsigned char*>(&pointsSize) + sizeof(pointsSize));

    for (const auto& point : points) {
      bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&point.first),
                   reinterpret_cast<const unsigned char*>(&point.first) + sizeof(point.first));
      bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&point.second),
                   reinterpret_cast<const unsigned char*>(&point.second) + sizeof(point.second));
    }

    // Serialize interpolation type
    auto typeVal = static_cast<int>(type);
    bytes.insert(bytes.end(), reinterpret_cast<const unsigned char*>(&typeVal),
                 reinterpret_cast<const unsigned char*>(&typeVal) + sizeof(typeVal));

    return bytes;
  }

  virtual void restore(Bytes::const_iterator& it) override {
    Operator<T, V>::restore(it);

    // Deserialize points
    size_t pointsSize = *reinterpret_cast<const size_t*>(&(*it));
    it += sizeof(pointsSize);

    points.clear();
    for (size_t i = 0; i < pointsSize; i++) {
      V x = *reinterpret_cast<const V*>(&(*it));
      it += sizeof(V);
      V y = *reinterpret_cast<const V*>(&(*it));
      it += sizeof(V);
      points.push_back({x, y});
    }

    // Deserialize interpolation type
    int typeVal = *reinterpret_cast<const int*>(&(*it));
    it += sizeof(typeVal);
    type = static_cast<InterpolationType>(typeVal);
  }

  string typeName() const override { return "Function"; }

  OperatorMessage<T, V> processData() override {
    string inputPort;
    auto in = this->getDataInputs();
    if (in.size() == 1)
      inputPort = in.at(0);
    else
      throw runtime_error(typeName() + " : more than 1 input port found");

    OperatorMessage<T, V> outputMsgs;
    Message<T, V> msg = this->getDataInputLastMessage(inputPort);
    V x = msg.value;

    size_t i = 0;
    while (i < points.size() - 1 && points[i + 1].first <= x) {
      i++;
    }

    if (type == InterpolationType::LINEAR) {
      if (i == 0 && x < points[0].first) {
        msg.value = linearInterpolate(points[0].first, points[0].second, points[1].first, points[1].second, x);
      } else if (i == points.size() - 1) {
        msg.value = linearInterpolate(points[i - 1].first, points[i - 1].second, points[i].first, points[i].second, x);
      } else {
        msg.value = linearInterpolate(points[i].first, points[i].second, points[i + 1].first, points[i + 1].second, x);
      }
    } else {
      if (i == 0 && x < points[0].first) {
        msg.value = hermiteInterpolate(points[0].second, points[1].second, getTangent(0), getTangent(1),
                                       (x - points[0].first) / (points[1].first - points[0].first));
      } else if (i >= points.size() - 2) {
        size_t last = points.size() - 1;
        msg.value =
            hermiteInterpolate(points[last - 1].second, points[last].second, getTangent(last - 1), getTangent(last),
                               (x - points[last - 1].first) / (points[last].first - points[last - 1].first));
      } else {
        msg.value = hermiteInterpolate(points[i].second, points[i + 1].second, getTangent(i), getTangent(i + 1),
                                       (x - points[i].first) / (points[i + 1].first - points[i].first));
      }
    }

    PortMessage<T, V> v;
    v.push_back(msg);
    outputMsgs.emplace("o1", v);
    return outputMsgs;
  }

  vector<pair<V, V>> getPoints() const { return points; }
  string getInterpolationType() const { return type == InterpolationType::LINEAR ? "linear" : "hermite"; }

 private:
  vector<pair<V, V>> points;
  InterpolationType type;

  static V linearInterpolate(V x1, V y1, V x2, V y2, V x) { return y1 + (y2 - y1) * (x - x1) / (x2 - x1); }

  static V hermiteInterpolate(V y0, V y1, V m0, V m1, V mu) {
    V mu2 = mu * mu;
    V mu3 = mu2 * mu;
    V h00 = 2 * mu3 - 3 * mu2 + 1;
    V h10 = mu3 - 2 * mu2 + mu;
    V h01 = -2 * mu3 + 3 * mu2;
    V h11 = mu3 - mu2;

    return h00 * y0 + h10 * m0 + h01 * y1 + h11 * m1;
  }

  V getTangent(size_t i) const {
    if (i == 0) {
      return (points[1].second - points[0].second) / (points[1].first - points[0].first);
    } else if (i == points.size() - 1) {
      return (points[i].second - points[i - 1].second) / (points[i].first - points[i - 1].first);
    } else {
      return (points[i + 1].second - points[i - 1].second) / (points[i + 1].first - points[i - 1].first);
    }
  }
};

}  // namespace rtbot

#endif  // FUNCTION_H