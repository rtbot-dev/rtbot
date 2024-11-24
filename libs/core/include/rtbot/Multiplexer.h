#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include <iostream>
#include <map>
#include <set>

#include "Operator.h"

namespace rtbot {

template <class T, class V>
class Multiplexer : public Operator<T, V> {
 public:
  Multiplexer() = default;
  Multiplexer(string const& id, size_t numPorts = 1) : Operator<T, V>(id) {
    if (numPorts < 1)
      throw std::runtime_error(typeName() + ": number of input ports have to be greater than or equal 1");

    for (int i = 1; i <= numPorts; i++) {
      string controlPort = string("c") + to_string(i);
      string inputPort = string("i") + to_string(i);
      this->addControlInput(controlPort);
      this->addDataInput(inputPort);
      this->controlMap.emplace(controlPort, inputPort);
    }
    this->addOutput("o1");  // Single output port
  }
  virtual ~Multiplexer() = default;

  virtual string typeName() const override { return "Multiplexer"; }

  void receiveData(Message<T, V> msg, string inputPort = "") override {
    if (inputPort.empty()) {
      throw std::runtime_error(typeName() + ": input port must be specified for Multiplexer");
    }

    if (this->dataInputs.count(inputPort) > 0) {
      this->dataInputs.find(inputPort)->second.push_back(msg);
      this->dataInputs.find(inputPort)->second.setSum(this->dataInputs.find(inputPort)->second.getSum() +
                                                      this->dataInputs.find(inputPort)->second.back().value);
    } else {
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing input data port");
    }
  }

  virtual void receiveControl(Message<T, V> msg, string inputPort) override {
    if (inputPort.empty()) {
      throw std::runtime_error(typeName() + ": control port must be specified for Multiplexer");
    }

    if (this->controlInputs.count(inputPort) > 0) {
      this->controlInputs.find(inputPort)->second.push_back(msg);
      this->controlInputs.find(inputPort)->second.setSum(this->controlInputs.find(inputPort)->second.getSum() +
                                                         this->controlInputs.find(inputPort)->second.back().value);

      // Track control message timestamps
      controlTimeTracker[msg.time]++;
      cout << "Incrementing control time tracker for timestamp " << msg.time << " to " << controlTimeTracker[msg.time]
           << endl;
    } else {
      throw std::runtime_error(typeName() + ": " + inputPort + " refers to a non existing control port");
    }
  }

  virtual ProgramMessage<T, V> executeData() override {
    auto toEmit = processData();
    if (!toEmit.empty()) return this->emit(toEmit);
    return {};
  }

  virtual ProgramMessage<T, V> executeControl() override {
    auto toEmit = processControl();
    if (!toEmit.empty()) return this->emit(toEmit);
    return {};
  }

  virtual OperatorMessage<T, V> processData() { return join(); }
  virtual OperatorMessage<T, V> processControl() { return join(); }

 private:
  map<string, string> controlMap;     // Maps control ports to input ports
  map<T, size_t> controlTimeTracker;  // Track number of control messages per timestamp

  OperatorMessage<T, V> join() {
    OperatorMessage<T, V> outputMsgs;

    while (true) {
      // Find the oldest timestamp that has all control messages
      auto oldestCommonTime = findOldestCommonControlTime();
      if (!oldestCommonTime.has_value()) {
        break;
      }

      // Clean up older control timestamps
      cleanOldControlTimestamps(*oldestCommonTime);

      // Find which input port should emit based on control values
      string portToEmit = findPortToEmit(*oldestCommonTime);
      if (portToEmit.empty()) {
        // Clean up this timestamp's control messages since they're invalid
        // This is a flaw in the program logic, so we shoud warn the user
        cerr << "Warning: " << typeName() << " (" << this->id << "): No control signal active" << endl;
        cleanupControlMessages(*oldestCommonTime);
        controlTimeTracker.erase(*oldestCommonTime);
        continue;
      }

      cout << "Port to emit: " << portToEmit << " at time " << *oldestCommonTime << endl;

      // Process the input queue of the selected port
      auto& msgs = this->dataInputs.find(portToEmit)->second;
      bool messageFound = false;

      // Remove messages with timestamps earlier than oldestCommonTime
      while (!msgs.empty() && msgs.front().time < *oldestCommonTime) {
        msgs.setSum(msgs.getSum() - msgs.front().value);
        msgs.pop_front();
      }

      // Check if we have a message with matching timestamp
      if (!msgs.empty() && msgs.front().time == *oldestCommonTime) {
        // Emit the message
        if (outputMsgs.find("o1") == outputMsgs.end()) {
          PortMessage<T, V> v;
          v.push_back(msgs.front());
          outputMsgs.emplace("o1", v);
        } else {
          outputMsgs.find("o1")->second.push_back(msgs.front());
        }

        // Clean up the emitted message
        msgs.setSum(msgs.getSum() - msgs.front().value);
        msgs.pop_front();
        messageFound = true;
      }

      // Clean up control messages for this timestamp
      cleanupControlMessages(*oldestCommonTime);
      controlTimeTracker.erase(*oldestCommonTime);

      // If we didn't find a matching message, finish the loop
      if (!messageFound) {
        break;
      }
    }

    return outputMsgs;
  }

  optional<T> findOldestCommonControlTime() {
    // we need to iterate over the controlTimeTracker map to find the oldest timestamp
    cout << "Control time tracker size: " << controlTimeTracker.size() << endl;
    cout << "Control time tracker keys: " << endl;
    for (const auto& pair : controlTimeTracker) {
      cout << pair.first << " ";
    }
    cout << endl;
    for (auto it = controlTimeTracker.begin(); it != controlTimeTracker.end(); ++it) {
      cout << "Timestamp: " << it->first << " Controls: " << it->second << endl;
      if (it->second == this->controlInputs.size()) {
        return it->first;
      }
    }

    return nullopt;
  }

  void cleanOldControlTimestamps(T currentTime) {
    vector<T> timesToRemove;
    for (const auto& pair : controlTimeTracker) {
      if (pair.first < currentTime) {
        // Clean up control messages for these timestamps
        cleanupControlMessages(pair.first);
        timesToRemove.push_back(pair.first);
      }
    }

    for (const auto& time : timesToRemove) {
      controlTimeTracker.erase(time);
    }
  }

  void cleanupControlMessages(T timestamp) {
    for (auto& ctrl : this->controlInputs) {
      while (!ctrl.second.empty() && ctrl.second.front().time <= timestamp) {
        ctrl.second.setSum(ctrl.second.getSum() - ctrl.second.front().value);
        ctrl.second.pop_front();
      }
    }
  }

  string findPortToEmit(T timestamp) {
    string selectedPort;
    int activeControls = 0;
    string activeControlPort;

    // Check control values for the timestamp
    for (const auto& ctrl : this->controlInputs) {
      if (!ctrl.second.empty() && ctrl.second.front().time == timestamp) {
        if (ctrl.second.front().value == 1) {
          activeControls++;
          activeControlPort = ctrl.first;
        } else if (ctrl.second.front().value != 0) {
          cerr << "Warning: " << typeName() << " (" << this->id << "): Invalid control value" << endl;
          return "";  // Invalid control values
        }
      }
    }

    // Exactly one control should be active
    if (activeControls == 1) {
      // Find the corresponding input port
      auto it = controlMap.find(activeControlPort);
      if (it != controlMap.end()) {
        selectedPort = it->second;
      }
    }

    return selectedPort;
  }
};

}  // end namespace rtbot

#endif  // MULTIPLEXER_H