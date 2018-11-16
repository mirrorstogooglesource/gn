// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_METADATA_H_
#define TOOLS_GN_METADATA_H_

#include <memory>

#include "tools/gn/scope.h"
#include "tools/gn/source_dir.h"

// Metadata about a particular target.
//
// Metadata is a collection of keys and values relating to a particular target.
// Generally, these keys will include three categories of strings: ordinary
// strings, filenames intended to be rebased according to their particular
// source directory, and target labels intended to be used as barriers to the
// walk. Verfication of these categories occurs at walk time, not creation
// time (since it is not clear until the walk which values are intended for
// which purpose).
//
// Represented as a scope in the expression language, here it is reduced to just
// the KeyValueMap (since it doesn't need the logical overhead of a full scope).
// Values must be lists of strings, as the walking collection logic contatenates
// their values across targets.
class Metadata {
 public:
  using Contents = Scope::KeyValueMap;

  // Members must be set explicitly.
  Metadata() = default;

  // The contents of this metadata varaiable.
  const Contents& contents() const { return contents_; }
  Contents& contents() { return contents_; }
  void set_contents(Contents&& contents) { contents_.swap(contents); }

  // The relative source directory to use when rebasing.
  const SourceDir& source_dir() const { return source_dir_; }
  SourceDir& source_dir() { return source_dir_; }
  void set_source_dir(const SourceDir& d) { source_dir_ = d; }

  // Collect the specified metadata from this instance.
  //
  // Calling this will populate `next_walk_keys` with the values of targets to
  // be walked next (with the empty string "" indicating that the target should
  // populate)
  void walk(const std::vector<base::StringPiece>& keys_to_extract,
            const std::vector<base::StringPiece>& keys_to_walk,
            std::vector<base::StringPiece>& next_walk_keys,
            std::vector<Value>& result,
            Err& err,
            bool rebase_files = false) const;

 private:
  Contents contents_;
  SourceDir source_dir_;

  DISALLOW_COPY_AND_ASSIGN(Metadata);
};

#endif  // TOOLS_GN_METADATA_H_
