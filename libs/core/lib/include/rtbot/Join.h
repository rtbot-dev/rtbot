#ifndef JOIN_H
#define JOIN_H

#include "Operator.h"

namespace rtbot {

using namespace std;

/**
 * @jsonschema
 * type: object
 * description: |
 *   `Join` operator is used to synchronize two or more incoming message streams into
 *   a single, consistent output.
 * properties:
 *   id:
 *     type: string
 *     description: The id of the operator
 *   numPorts:
 *     type: integer
 *     description: The number of input ports.
 *     default: 1
 *     minimum: 1
 *   policies:
 *     type: object
 *     patternProperties:
 *       # any valid operator id
 *       "^[a-zA-Z0-9]+$":
 *          type: object
 *          additionalProperties: false
 *          properties:
 *            eager:
 *              type: boolean
 *              default: false
 *
 * required: ["id"]
 */
template <class T, class V>
class Join : public Operator<T, V> {
 public:
  Join() = default;
  Join(string const &id) : Operator<T, V>(id) {}
  Join(string const &id, size_t numPorts, map<string, typename Operator<T, V>::InputPolicy> policies = {})
      : Operator<T, V>(id) {
    if (numPorts < 2) throw runtime_error(typeName() + ": number of ports have to be greater than or equal 2");

    this->notEagerPort = "";
    this->eagerPort = "";
    for (int i = 1; i <= numPorts; i++) {
      string inputPort = string("i") + to_string(i);
      string outputPort = string("o") + to_string(i);
      if (policies.count(inputPort) > 0) {
        if (policies.find(inputPort)->second.isEager())
          if (this->eagerPort.empty())
            this->eagerPort = inputPort;
          else
            throw runtime_error(typeName() + ": 2 or more eager ports are not allowed");
        else
          this->notEagerPort = inputPort;
        this->addDataInput(inputPort, 0, policies.find(inputPort)->second);
      } else {
        this->addDataInput(inputPort, 0, {});
        this->notEagerPort = inputPort;
      }
      this->addOutput(outputPort);
      this->controlMap.emplace(inputPort, outputPort);
    }
    if (this->notEagerPort.empty()) throw runtime_error(typeName() + ": at least one input port should be not eager");
  }
  virtual ~Join() = default;

  virtual string typeName() const override { return "Join"; }

  void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      throw runtime_error(typeName() + " : inputPort have to be specified");
    }

    if (this->dataInputs.count(inputPort) > 0) {
      if (this->dataInputs.find(inputPort)->second.isEager() && !this->dataInputs.find(inputPort)->second.empty()) {
        this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() -
                                                        this->dataInputs.find(inputPort)->second.front().value);
        this->dataInputs.find(inputPort)->second.pop_front();
      }
      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
    } else
      throw runtime_error(typeName() + ": " + inputPort + " refers to a non existing input port");
  }

  virtual map<string, map<string, vector<Message<T, V>>>> executeData() override {
    this->outputMsgs.clear();

    checkReady();

    if (!this->outputMsgs.empty()) {
      return this->emit(this->outputMsgs);
    } else
      return {};
  }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  map<string, vector<Message<T, V>>> checkReady() {
    string inputPort = this->notEagerPort;
    if (this->dataInputs.find(inputPort)->second.empty()) return {};
    T latest = this->dataInputs.find(inputPort)->second.front().time;

    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->second.empty())
        return {};
      else if (it->second.front().time > latest && !it->second.isEager()) {
        inputPort = it->first;
        latest = it->second.front().time;
      }
    }

    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->first == inputPort || it->second.isEager()) continue;
      while (!it->second.empty() && (it->second.front().time < this->dataInputs.find(inputPort)->second.front().time)) {
        it->second.setSum(it->second.getSum() - it->second.front().value);
        it->second.pop_front();
      }
    }

    bool all_ready = true;
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      if (it->second.empty() ||
          (it->second.front().time > this->dataInputs.find(inputPort)->second.front().time && !it->second.isEager())) {
        all_ready = false;
        break;
      }
    }

    if (all_ready) {
      processData();
      for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
        if (!it->second.isEager()) {
          it->second.setSum(it->second.getSum() - it->second.front().value);
          it->second.pop_front();
        }
      }
      if (!this->eagerPort.empty()) {
        checkReady();
      }
    }
    return {};
  }

  /*
    map<outputPort, vector<Message<T, V>>>
  */
  virtual map<string, vector<Message<T, V>>> processData() {
    for (auto it = this->dataInputs.begin(); it != this->dataInputs.end(); ++it) {
      Message<T, V> out = it->second.front();
      out.time = this->dataInputs.find(this->notEagerPort)->second.front().time;
      if (this->outputMsgs.count(this->controlMap.find(it->first)->second) == 0) {
        vector<Message<T, V>> v;
        v.push_back(out);
        this->outputMsgs.emplace(this->controlMap.find(it->first)->second, v);
      } else
        this->outputMsgs[this->controlMap.find(it->first)->second].push_back(out);
    }

    return {};
  }

 protected:
  string notEagerPort;
  string eagerPort;
  map<string, string> controlMap;
  map<string, vector<Message<T, V>>> outputMsgs;
};

}  // end namespace rtbot

#endif  // JOIN_H
