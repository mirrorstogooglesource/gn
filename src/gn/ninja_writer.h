// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_WRITER_H_
#define TOOLS_GN_NINJA_WRITER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

class Builder;
class BuildSettings;
class Err;
class Target;
class Toolchain;

class NinjaWriter {
 public:
  // Combines a target and the computed build rule for it.
  using TargetRulePair = std::pair<const Target*, std::string>;

  // Associates the build rules with each toolchain.
  using PerToolchainRules =
      std::map<const Toolchain*, std::vector<TargetRulePair>>;

  // On failure will populate |err| and will return false.  The map contains
  // the per-toolchain set of rules collected to write to the toolchain build
  // files. The is_regeneration flag is passed along to
  // NinjaBuildWriter::RunAndWriteFile.
  static bool RunAndWriteFiles(const BuildSettings* build_settings,
                               const Builder& builder,
                               const PerToolchainRules& per_toolchain_rules,
                               bool is_regeneration,
                               Err* err);

 private:
  NinjaWriter(const Builder& builder);
  ~NinjaWriter();

  bool WriteToolchains(const PerToolchainRules& per_toolchain_rules, Err* err);

  const Builder& builder_;

  NinjaWriter(const NinjaWriter&) = delete;
  NinjaWriter& operator=(const NinjaWriter&) = delete;
};

#endif  // TOOLS_GN_NINJA_WRITER_H_
