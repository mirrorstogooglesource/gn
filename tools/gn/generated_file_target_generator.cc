// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/generated_file_target_generator.h"

#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/target.h"
#include "tools/gn/variables.h"

GeneratedFileTargetGenerator::GeneratedFileTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Target::OutputType type,
    Err* err)
    : TargetGenerator(target, scope, function_call, err),
      output_type_(type),
      contents_defined_(false),
      data_keys_defined_(false) {}

GeneratedFileTargetGenerator::~GeneratedFileTargetGenerator() = default;

void GeneratedFileTargetGenerator::DoRun() {
  target_->set_output_type(output_type_);

  if (!FillOutputs(false))
    return;
  if (target_->action_values().outputs().list().size() != 1) {
    *err_ = Err(
        function_call_, "generated_file target must have exactly one output.",
        "You must specify exactly one value in the \"outputs\" array for the "
        "destination of the write\n(see \"gn help generated_file\").");
    return;
  }

  if (!FillContents())
    return;
  if (!FillDataKeys())
    return;

  // One of data and data_keys should be defined.
  if (!contents_defined_ && !data_keys_defined_) {
    *err_ = Err(
        function_call_, "Either contents or data_keys should be set.",
        "The generated_file target requires either the \"contents\" variable "
        "or the \"data_keys\" variable be set. See \"gn help "
        "generated_file\".");
    return;
  }

  if (!FillRebase())
    return;
  if (!FillWalkKeys())
    return;

  if (!FillOutputConversion())
    return;
}

bool GeneratedFileTargetGenerator::FillContents() {
  const Value* value = scope_->GetValue(variables::kWriteValueContents, true);
  if (!value)
    return true;
  target_->set_contents(*value);
  contents_defined_ = true;
  return true;
}

bool GeneratedFileTargetGenerator::ContentsDefined(
    const base::StringPiece& name,
    const ParseNode* origin) {
  if (contents_defined_) {
    *err_ =
        Err(origin, name.as_string() + " won't be used.",
            "\"contents\" is defined on this target, and so setting " +
                name.as_string() +
                " will have no effect as no metdata collection will occur.");
    return true;
  }
  return false;
}

bool GeneratedFileTargetGenerator::FillOutputConversion() {
  const Value* value =
      scope_->GetValue(variables::kWriteOutputConversion, true);
  if (!value) {
    target_->set_output_conversion(Value(function_call_, ""));
    return true;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  // Otherwise, the value itself will be checked when the conversion is done.
  target_->set_output_conversion(*value);
  return true;
}

bool GeneratedFileTargetGenerator::FillRebase() {
  const Value* value = scope_->GetValue(variables::kRebase, true);
  if (!value)
    return true;
  if (ContentsDefined(variables::kRebase, value->origin()))
    return false;
  if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
    return false;
  target_->set_rebase(value->boolean_value());
  return true;
}

bool GeneratedFileTargetGenerator::FillDataKeys() {
  const Value* value = scope_->GetValue(variables::kDataKeys, true);
  if (!value)
    return true;
  if (ContentsDefined(variables::kDataKeys, value->origin()))
    return false;
  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  for (const Value& v : value->list_value()) {
    // Keys must be strings.
    if (!v.VerifyTypeIs(Value::STRING, err_))
      return false;
    target_->data_keys().push_back(v.string_value());
  }

  data_keys_defined_ = true;
  return true;
}

bool GeneratedFileTargetGenerator::FillWalkKeys() {
  const Value* value = scope_->GetValue(variables::kWalkKeys, true);
  // If we define this and contents, that's an error.
  if (value && ContentsDefined(variables::kWalkKeys, value->origin()))
    return false;

  // If we don't define it, we want the default value which is a list
  // containing the empty string.
  if (!value) {
    target_->walk_keys().push_back("");
    return true;
  }

  // Otherwise, pull and validate the specified value.
  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;
  for (const Value& v : value->list_value()) {
    // Keys must be strings.
    if (!v.VerifyTypeIs(Value::STRING, err_))
      return false;
    target_->walk_keys().push_back(v.string_value());
  }
  return true;
}
