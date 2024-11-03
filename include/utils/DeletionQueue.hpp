#pragma once
#include <stack>

struct DeletionQueue {
  std::stack<std::function<void()>> delete_callbacks;
  void push(std::function<void()> &&function) {
    delete_callbacks.push(function);
  }
  void flush() {
    while (!delete_callbacks.empty()) {
      delete_callbacks.top()();
      delete_callbacks.pop();
    }
  }
};