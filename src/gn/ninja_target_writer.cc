// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_target_writer.h"

#include <sstream>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "gn/builtin_tool.h"
#include "gn/config_values_extractors.h"
#include "gn/err.h"
#include "gn/escape.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_action_target_writer.h"
#include "gn/ninja_binary_target_writer.h"
#include "gn/ninja_bundle_data_target_writer.h"
#include "gn/ninja_copy_target_writer.h"
#include "gn/ninja_create_bundle_target_writer.h"
#include "gn/ninja_generated_file_target_writer.h"
#include "gn/ninja_group_target_writer.h"
#include "gn/ninja_utils.h"
#include "gn/output_file.h"
#include "gn/scheduler.h"
#include "gn/string_output_buffer.h"
#include "gn/string_utils.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/trace.h"

NinjaTargetWriter::NinjaTargetWriter(const Target* target, std::ostream& out)
    : settings_(target->settings()),
      target_(target),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   settings_->build_settings()->root_path_utf8(),
                   ESCAPE_NINJA) {}

NinjaTargetWriter::~NinjaTargetWriter() = default;

// static
std::string NinjaTargetWriter::RunAndWriteFile(const Target* target) {
  const Settings* settings = target->settings();

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE,
                    target->label().GetUserVisibleName(false));
  trace.SetToolchain(settings->toolchain_label());

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Computing", target->label().GetUserVisibleName(true));

  // It's ridiculously faster to write to a string and then write that to
  // disk in one operation than to use an fstream here.
  StringOutputBuffer storage;
  std::ostream rules(&storage);

  // Call out to the correct sub-type of writer. Binary targets need to be
  // written to separate files for compiler flag scoping, but other target
  // types can have their rules coalesced.
  //
  // In ninja, if a rule uses a variable (like $include_dirs) it will use
  // the value set by indenting it under the build line or it takes the value
  // from the end of the invoking scope (otherwise the current file). It does
  // not copy the value from what it was when the build line was encountered.
  // To avoid writing lots of duplicate rules for defines and cflags, etc. on
  // each source file build line, we use separate .ninja files with the shared
  // variables set at the top.
  //
  // Groups and actions don't use this type of flag, they make unique rules
  // or write variables scoped under each build line. As a result, they don't
  // need the separate files.
  bool needs_file_write = false;
  if (target->output_type() == Target::BUNDLE_DATA) {
    NinjaBundleDataTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::CREATE_BUNDLE) {
    NinjaCreateBundleTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::COPY_FILES) {
    NinjaCopyTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::ACTION ||
             target->output_type() == Target::ACTION_FOREACH) {
    NinjaActionTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::GROUP) {
    NinjaGroupTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::GENERATED_FILE) {
    NinjaGeneratedFileTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->IsBinary()) {
    needs_file_write = true;
    NinjaBinaryTargetWriter writer(target, rules);
    writer.Run();
  } else {
    CHECK(0) << "Output type of target not handled.";
  }

  if (needs_file_write) {
    // Write the ninja file.
    SourceFile ninja_file = GetNinjaFileForTarget(target);
    base::FilePath full_ninja_file =
        settings->build_settings()->GetFullPath(ninja_file);
    storage.WriteToFileIfChanged(full_ninja_file, nullptr);

    EscapeOptions options;
    options.mode = ESCAPE_NINJA;

    // Return the subninja command to load the rules file.
    std::string result = "subninja ";
    result.append(EscapeString(
        OutputFile(target->settings()->build_settings(), ninja_file).value(),
        options, nullptr));
    result.push_back('\n');
    return result;
  }

  // No separate file required, just return the rules.
  return storage.str();
}

void NinjaTargetWriter::WriteEscapedSubstitution(const Substitution* type) {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA;

  out_ << type->ninja_name << " = ";
  EscapeStringToStream(
      out_, SubstitutionWriter::GetTargetSubstitution(target_, type), opts);
  out_ << std::endl;
}

void NinjaTargetWriter::WriteSharedVars(const SubstitutionBits& bits) {
  bool written_anything = false;

  // Target label.
  if (bits.used.count(&SubstitutionLabel)) {
    WriteEscapedSubstitution(&SubstitutionLabel);
    written_anything = true;
  }

  // Target label name.
  if (bits.used.count(&SubstitutionLabelName)) {
    WriteEscapedSubstitution(&SubstitutionLabelName);
    written_anything = true;
  }

  // Target label name without toolchain.
  if (bits.used.count(&SubstitutionLabelNoToolchain)) {
    WriteEscapedSubstitution(&SubstitutionLabelNoToolchain);
    written_anything = true;
  }

  // Root gen dir.
  if (bits.used.count(&SubstitutionRootGenDir)) {
    WriteEscapedSubstitution(&SubstitutionRootGenDir);
    written_anything = true;
  }

  // Root out dir.
  if (bits.used.count(&SubstitutionRootOutDir)) {
    WriteEscapedSubstitution(&SubstitutionRootOutDir);
    written_anything = true;
  }

  // Target gen dir.
  if (bits.used.count(&SubstitutionTargetGenDir)) {
    WriteEscapedSubstitution(&SubstitutionTargetGenDir);
    written_anything = true;
  }

  // Target out dir.
  if (bits.used.count(&SubstitutionTargetOutDir)) {
    WriteEscapedSubstitution(&SubstitutionTargetOutDir);
    written_anything = true;
  }

  // Target output name.
  if (bits.used.count(&SubstitutionTargetOutputName)) {
    WriteEscapedSubstitution(&SubstitutionTargetOutputName);
    written_anything = true;
  }

  // If we wrote any vars, separate them from the rest of the file that follows
  // with a blank line.
  if (written_anything)
    out_ << std::endl;
}

std::vector<OutputFile> NinjaTargetWriter::WriteInputDepsPhonyAndGetDep(
    const std::vector<const Target*>& additional_hard_deps,
    size_t num_output_uses) const {
  CHECK(target_->toolchain()) << "Toolchain not set on target "
                              << target_->label().GetUserVisibleName(true);

  // ----------
  // Collect all input files that are input deps of this target. Knowing the
  // number before writing allows us to either skip writing the input deps
  // phony or optimize it. Use pointers to avoid copies here.
  std::vector<const SourceFile*> input_deps_sources;
  input_deps_sources.reserve(32);

  // Actions get implicit dependencies on the script itself.
  if (target_->output_type() == Target::ACTION ||
      target_->output_type() == Target::ACTION_FOREACH)
    input_deps_sources.push_back(&target_->action_values().script());

  // Input files are only considered for non-binary targets which use an
  // implicit dependency instead. The implicit dependency in this case is
  // handled separately by the binary target writer.
  if (!target_->IsBinary()) {
    for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
      for (const auto& input : iter.cur().inputs())
        input_deps_sources.push_back(&input);
    }
  }

  // For an action (where we run a script only once) the sources are the same
  // as the inputs. For action_foreach, the sources will be operated on
  // separately so don't handle them here.
  if (target_->output_type() == Target::ACTION) {
    for (const auto& source : target_->sources())
      input_deps_sources.push_back(&source);
  }

  // ----------
  // Collect all target input dependencies of this target as was done for the
  // files above.
  std::vector<const Target*> input_deps_targets;
  input_deps_targets.reserve(32);

  // Hard dependencies that are direct or indirect dependencies.
  // These are large (up to 100s), hence why we check other
  const std::set<const Target*>& hard_deps(target_->recursive_hard_deps());
  for (const Target* target : hard_deps) {
    // BUNDLE_DATA should normally be treated as a data-only dependency
    // (see Target::IsDataOnly()). Only the CREATE_BUNDLE target, that actually
    // consumes this data, needs to have the BUNDLE_DATA as an input dependency.
    if (target->output_type() != Target::BUNDLE_DATA ||
        target_->output_type() == Target::CREATE_BUNDLE)
      input_deps_targets.push_back(target);
  }

  // Additional hard dependencies passed in. These are usually empty or small,
  // and we don't want to duplicate the explicit hard deps of the target.
  for (const Target* target : additional_hard_deps) {
    if (!hard_deps.count(target))
      input_deps_targets.push_back(target);
  }

  // Toolchain dependencies. These must be resolved before doing anything.
  // This just writes all toolchain deps for simplicity. If we find that
  // toolchains often have more than one dependency, we could consider writing
  // a toolchain-specific phony target and only include the phony here.
  // Note that these are usually empty/small.
  const LabelTargetVector& toolchain_deps = target_->toolchain()->deps();
  for (const auto& toolchain_dep : toolchain_deps) {
    // This could theoretically duplicate dependencies already in the list,
    // but it shouldn't happen in practice, is inconvenient to check for,
    // and only results in harmless redundant dependencies listed.
    input_deps_targets.push_back(toolchain_dep.ptr);
  }

  // ---------
  // Write the outputs.

  if (input_deps_sources.size() + input_deps_targets.size() == 0)
    return std::vector<OutputFile>();  // No input dependencies.

  // If we're only generating one input dependency, return it directly instead
  // of writing a phony target for it.
  if (input_deps_sources.size() == 1 && input_deps_targets.size() == 0)
    return std::vector<OutputFile>{
        OutputFile(settings_->build_settings(), *input_deps_sources[0])};
  if (input_deps_sources.size() == 0 && input_deps_targets.size() == 1) {
    const std::optional<OutputFile>& dep =
        input_deps_targets[0]->dependency_output_file_or_phony();
    if (!dep)
      return std::vector<OutputFile>();
    return std::vector<OutputFile>{*dep};
  }

  std::vector<OutputFile> outs;
  // File input deps.
  for (const SourceFile* source : input_deps_sources)
    outs.push_back(OutputFile(settings_->build_settings(), *source));
  // Target input deps. Sort by label so the output is deterministic (otherwise
  // some of the targets will have gone through std::sets which will have
  // sorted them by pointer).
  std::sort(
      input_deps_targets.begin(), input_deps_targets.end(),
      [](const Target* a, const Target* b) { return a->label() < b->label(); });
  for (auto* dep : input_deps_targets) {
    if (dep->dependency_output_file_or_phony())
      outs.push_back(*dep->dependency_output_file_or_phony());
  }

  // If there are multiple inputs, but the phony target would be referenced only
  // once, don't write it but depend on the inputs directly.
  if (num_output_uses == 1u)
    return outs;

  // Make a phony target. We don't need to worry about an empty phony target, as
  // we would return early if there were no inputs.
  CHECK(!outs.empty());
  OutputFile input_phony_file =
      GetBuildDirForTargetAsOutputFile(target_, BuildDirType::PHONY);
  input_phony_file.value().append(target_->label().name());
  input_phony_file.value().append(".inputdeps");

  out_ << "build ";
  path_output_.WriteFile(out_, input_phony_file);
  out_ << ": " << BuiltinTool::kBuiltinToolPhony;
  path_output_.WriteFiles(out_, outs);

  out_ << "\n";
  return std::vector<OutputFile>{input_phony_file};
}

void NinjaTargetWriter::WritePhonyForTarget(
    const std::vector<OutputFile>& files,
    const std::vector<OutputFile>& order_only_deps) {
  // If there's no phony, then we should not have any inputs and it is okay to
  // omit the build rule.
  if (!target_->dependency_output_phony()) {
    CHECK(files.empty());
    CHECK(order_only_deps.empty());
    return;
  }
  const OutputFile& phony_target = *target_->dependency_output_phony();
  CHECK(!phony_target.value().empty());

  out_ << "build ";
  path_output_.WriteFile(out_, phony_target);

  out_ << ": " << BuiltinTool::kBuiltinToolPhony;
  path_output_.WriteFiles(out_, files);

  if (!order_only_deps.empty()) {
    out_ << " ||";
    path_output_.WriteFiles(out_, order_only_deps);
  }
  out_ << std::endl;
}
