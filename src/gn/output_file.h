// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_OUTPUT_FILE_H_
#define TOOLS_GN_OUTPUT_FILE_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

class BuildSettings;
class SourceDir;
class SourceFile;

// A simple wrapper around a string that indicates the string is a path
// relative to the output directory.
class OutputFile {
 public:
  OutputFile() = default;

  explicit OutputFile(std::string&& v);
  explicit OutputFile(const std::string& v);

  OutputFile(const BuildSettings* build_settings,
             const SourceFile& source_file);

  std::string& value() { return value_; }
  const std::string& value() const { return value_; }

  // Converts to a SourceFile by prepending the build directory to the file.
  // The *Dir version requires that the current OutputFile ends in a slash, and
  // the *File version is the opposite.
  SourceFile AsSourceFile(const BuildSettings* build_settings) const;
  SourceDir AsSourceDir(const BuildSettings* build_settings) const;

  bool operator==(const OutputFile& other) const {
    return value_ == other.value_;
  }
  bool operator!=(const OutputFile& other) const {
    return value_ != other.value_;
  }
  bool operator<(const OutputFile& other) const {
    return value_ < other.value_;
  }

 private:
  std::string value_;
};

// A helper around std::set<> for OutputFile that can be easily created from a
// vector or converted into a vector of sorted items.
class OutputFileSet : public std::set<OutputFile> {
  public:
  OutputFileSet() = default;

  // Creates a new OutputFileSet initialized with the contents of the passed-in
  // vector.
  explicit OutputFileSet(const std::vector<OutputFile>& v);

  // Adds all items from the passed-in vector to this OutputFileSet.
  void InsertAll(const std::vector<OutputFile>& v);

  // Returns true if this set contains the given file.
  bool contains(const OutputFile& v);

  // Returns a vector containing all the OutputFiles in this set, in sorted
  // order.
  std::vector<OutputFile> AsSortedVector();
};

namespace std {

template <>
struct hash<OutputFile> {
  std::size_t operator()(const OutputFile& v) const {
    hash<std::string> h;
    return h(v.value());
  }
};

}  // namespace std

#endif  // TOOLS_GN_OUTPUT_FILE_H_
