// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "ExceptionsManagerModule.h"
#include <assert.h>
#include <cxxreact/JsArgumentHelpers.h>
#include <sstream>

namespace facebook {
namespace react {

namespace {
std::string RetrieveStringFromMap(const folly::dynamic &map, const std::string &key) noexcept {
  assert(map.type() == folly::dynamic::OBJECT);
  auto iterator = map.find(key);
  if (iterator != map.items().end()) {
    return iterator->second.asString();
  }
  assert(false);
  return {};
}

int RetrieveIntFromMap(const folly::dynamic &map, const std::string &key) noexcept {
  assert(map.type() == folly::dynamic::OBJECT);

  auto iterator = map.find(key);
  if (iterator != map.items().end()) {
    if (iterator->second.isNull()) {
      return -1;
    }
    assert(iterator->second.isNumber());
    return static_cast<int>(iterator->second.asDouble());
  }
  assert(false);
  return -1;
}

Mso::React::ErrorInfo CreateErrorInfo(const folly::dynamic &args) noexcept {
  // Parameter args is a dynamic array containing 3 objects:
  // 1. an exception message string.
  // 2. an array containing stack information.
  // 3. an exceptionID int.
  assert(args.isArray());
  assert(args.size() == 3);
  assert(args[0].isString());
  assert(args[1].isArray());
  assert(args[2].isNumber());
  assert(facebook::xplat::jsArgAsInt(args, 2) <= std::numeric_limits<uint32_t>::max());

  Mso::React::ErrorInfo errorInfo;
  errorInfo.Message = facebook::xplat::jsArgAsString(args, 0);
  errorInfo.Id = static_cast<uint32_t>(facebook::xplat::jsArgAsInt(args, 2));

  folly::dynamic stackAsFolly = facebook::xplat::jsArgAsArray(args, 1);

  // Construct a string containing the stack frame info in the following format:
  // <method> Line:<Line Number>  Column:<ColumnNumber> <Filename>
  for (const auto &stackFrame : stackAsFolly) {
    // Each dynamic object is a map containing information about the stack
    // frame: method (string), arguments (array), filename(string), line number
    // (int) and column number (int).
    assert(stackFrame.isObject());
    assert(stackFrame.size() >= 4); // 4 in 0.57, 5 in 0.58+ (arguments added)

    errorInfo.Callstack.push_back(Mso::React::ErrorFrameInfo{RetrieveStringFromMap(stackFrame, "file"),
                                                             RetrieveStringFromMap(stackFrame, "methodName"),
                                                             RetrieveIntFromMap(stackFrame, "lineNumber"),
                                                             RetrieveIntFromMap(stackFrame, "column")});
  }

  return errorInfo;
}

} // namespace

ExceptionsManagerModule::ExceptionsManagerModule(std::shared_ptr<Mso::React::IRedBoxHandler> redboxHandler)
    : m_redboxHandler(std::move(redboxHandler)) {}

std::string ExceptionsManagerModule::getName() {
  return name;
}

std::map<std::string, folly::dynamic> ExceptionsManagerModule::getConstants() {
  return std::map<std::string, folly::dynamic>();
}

std::vector<facebook::xplat::module::CxxModule::Method> ExceptionsManagerModule::getMethods() {
  return {Method(
              "reportFatalException",
              [this](folly::dynamic args) noexcept {
                if (m_redboxHandler && m_redboxHandler->isDevSupportEnabled()) {
                  m_redboxHandler->showNewError(std::move(CreateErrorInfo(args)), Mso::React::ErrorType::JSFatal);
                }
                /*
                // TODO - fatal errors should throw if there is no redbox handler
                else {
                  throw Exception();
                } */
              }),

          Method(
              "reportSoftException",
              [this](folly::dynamic args) noexcept {
                if (m_redboxHandler && m_redboxHandler->isDevSupportEnabled()) {
                  m_redboxHandler->showNewError(std::move(CreateErrorInfo(args)), Mso::React::ErrorType::JSSoft);
                }
              }),

          Method(
              "updateExceptionMessage",
              [this](folly::dynamic args) noexcept {
                if (m_redboxHandler && m_redboxHandler->isDevSupportEnabled()) {
                  m_redboxHandler->updateError(std::move(CreateErrorInfo(args)));
                }
              }),

          Method("dismissRedbox", [this](folly::dynamic /*args*/) noexcept {
            if (m_redboxHandler)
              m_redboxHandler->dismissRedbox();
          })};
}

} // namespace react
} // namespace facebook
