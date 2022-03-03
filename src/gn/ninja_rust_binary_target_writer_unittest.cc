// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_rust_binary_target_writer.h"

#include "gn/config.h"
#include "gn/rust_values.h"
#include "gn/scheduler.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

using NinjaRustBinaryTargetWriterTest = TestWithScheduler;

TEST_F(NinjaRustBinaryTargetWriterTest, RustSourceSet) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::SOURCE_SET);
  target.visibility().SetPublic();
  target.sources().push_back(SourceFile("//foo/input1.rs"));
  target.sources().push_back(SourceFile("//foo/main.rs"));
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.SetToolchain(setup.toolchain());
  ASSERT_FALSE(target.OnResolved(&err));
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustExecutable) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input3.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.config_values().ldflags().push_back("-fsanitize=address");
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/input3.rs "
        "../../foo/main.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags = -fsanitize=address\n"
        "  sources = ../../foo/input3.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

// Accessible dependencies appear as --extern switches for rustc, so that the
// target crate can make use of them whether transitive or not. Transitive
// dependencies can be accessible if they are in the public_deps of a direct
// dependency, or part of a chain of public_deps from a direct dependency. Any
// dependencies used by other crate dependencies also must appear, but are
// pointed to by -Ldependency as they are not available for use from the target
// crate. In the future they may move to `--extern priv:` when explicit private
// dependencies are stabilized.
TEST_F(NinjaRustBinaryTargetWriterTest, RlibDeps) {
  Err err;
  TestWithScope setup;

  Target private_rlib(setup.settings(), Label(SourceDir("//baz/"), "privatelib"));
  private_rlib.set_output_type(Target::RUST_LIBRARY);
  private_rlib.visibility().SetPublic();
  SourceFile bazlib("//baz/lib.rs");
  private_rlib.sources().push_back(SourceFile("//baz/privatelib.rs"));
  private_rlib.sources().push_back(bazlib);
  private_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  private_rlib.rust_values().set_crate_root(bazlib);
  private_rlib.rust_values().crate_name() = "privatecrate";
  private_rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(private_rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&private_rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = privatecrate\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/baz\n"
        "target_output_name = libprivatelib\n"
        "\n"
        "build obj/baz/libprivatelib.rlib: rust_rlib ../../baz/lib.rs | "
        "../../baz/privatelib.rs ../../baz/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../baz/privatelib.rs ../../baz/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target far_public_rlib(setup.settings(),
                         Label(SourceDir("//far/"), "farlib"));
  far_public_rlib.set_output_type(Target::RUST_LIBRARY);
  far_public_rlib.visibility().SetPublic();
  SourceFile farlib("//far/lib.rs");
  far_public_rlib.sources().push_back(SourceFile("//far/farlib.rs"));
  far_public_rlib.sources().push_back(farlib);
  far_public_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  far_public_rlib.rust_values().set_crate_root(farlib);
  far_public_rlib.rust_values().crate_name() = "farcrate";
  far_public_rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(far_public_rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&far_public_rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = farcrate\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/far\n"
        "target_output_name = libfarlib\n"
        "\n"
        "build obj/far/libfarlib.rlib: rust_rlib ../../far/lib.rs | "
        "../../far/farlib.rs ../../far/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../far/farlib.rs ../../far/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target public_rlib(setup.settings(), Label(SourceDir("//bar/"), "publiclib"));
  public_rlib.set_output_type(Target::RUST_LIBRARY);
  public_rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  public_rlib.sources().push_back(SourceFile("//bar/publiclib.rs"));
  public_rlib.sources().push_back(barlib);
  public_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  public_rlib.rust_values().set_crate_root(barlib);
  public_rlib.rust_values().crate_name() = "publiccrate";
  public_rlib.public_deps().push_back(LabelTargetPair(&far_public_rlib));
  public_rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(public_rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&public_rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = publiccrate\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libpubliclib\n"
        "\n"
        "build obj/bar/libpubliclib.rlib: rust_rlib ../../bar/lib.rs | "
        "../../bar/publiclib.rs ../../bar/lib.rs obj/far/libfarlib.rlib\n"
        "  externs = --extern farcrate=obj/far/libfarlib.rlib\n"
        "  rustdeps = -Ldependency=obj/far\n"
        "  ldflags =\n"
        "  sources = ../../bar/publiclib.rs ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target rlib(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile lib("//foo/main.rs");
  rlib.sources().push_back(SourceFile("//foo/direct.rs"));
  rlib.sources().push_back(lib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(lib);
  rlib.rust_values().crate_name() = "direct";
  rlib.SetToolchain(setup.toolchain());
  rlib.public_deps().push_back(LabelTargetPair(&public_rlib));
  rlib.private_deps().push_back(LabelTargetPair(&private_rlib));
  ASSERT_TRUE(rlib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//main/"), "main"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//main/main.rs");
  target.sources().push_back(SourceFile("//main/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "main_crate";
  target.private_deps().push_back(LabelTargetPair(&rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = main_crate\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/main\n"
        "target_output_name = main\n"
        "\n"
        "build ./main_crate: rust_bin ../../main/main.rs | "
        "../../main/source.rs ../../main/main.rs obj/foo/libdirect.rlib\n"
        "  externs = --extern direct=obj/foo/libdirect.rlib "
        "--extern publiccrate=obj/bar/libpubliclib.rlib "
        "--extern farcrate=obj/far/libfarlib.rlib\n"
        "  rustdeps = -Ldependency=obj/foo -Ldependency=obj/bar "
        "-Ldependency=obj/far -Ldependency=obj/baz\n"
        "  ldflags =\n"
        "  sources = ../../main/source.rs ../../main/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, DylibDeps) {
  Err err;
  TestWithScope setup;

  Target private_inside_dylib(setup.settings(), Label(SourceDir("//faz/"), "private_inside"));
  private_inside_dylib.set_output_type(Target::RUST_LIBRARY);
  private_inside_dylib.visibility().SetPublic();
  SourceFile fazlib("//faz/lib.rs");
  private_inside_dylib.sources().push_back(SourceFile("//faz/private_inside.rs"));
  private_inside_dylib.sources().push_back(fazlib);
  private_inside_dylib.source_types_used().Set(SourceFile::SOURCE_RS);
  private_inside_dylib.rust_values().set_crate_root(fazlib);
  private_inside_dylib.rust_values().crate_name() = "private_inside";
  private_inside_dylib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(private_inside_dylib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&private_inside_dylib, out);
    writer.Run();

    const char expected[] =
        "crate_name = private_inside\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/faz\n"
        "target_output_name = libprivate_inside\n"
        "\n"
        "build obj/faz/libprivate_inside.rlib: rust_rlib ../../faz/lib.rs | "
        "../../faz/private_inside.rs ../../faz/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../faz/private_inside.rs ../../faz/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target inside_dylib(setup.settings(), Label(SourceDir("//baz/"), "inside"));
  inside_dylib.set_output_type(Target::RUST_LIBRARY);
  inside_dylib.visibility().SetPublic();
  SourceFile bazlib("//baz/lib.rs");
  inside_dylib.sources().push_back(SourceFile("//baz/inside.rs"));
  inside_dylib.sources().push_back(bazlib);
  inside_dylib.source_types_used().Set(SourceFile::SOURCE_RS);
  inside_dylib.rust_values().set_crate_root(bazlib);
  inside_dylib.rust_values().crate_name() = "inside";
  inside_dylib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(inside_dylib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&inside_dylib, out);
    writer.Run();

    const char expected[] =
        "crate_name = inside\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/baz\n"
        "target_output_name = libinside\n"
        "\n"
        "build obj/baz/libinside.rlib: rust_rlib ../../baz/lib.rs | "
        "../../baz/inside.rs ../../baz/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../baz/inside.rs ../../baz/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target dylib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  dylib.set_output_type(Target::SHARED_LIBRARY);
  dylib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  dylib.sources().push_back(SourceFile("//bar/mylib.rs"));
  dylib.sources().push_back(barlib);
  dylib.source_types_used().Set(SourceFile::SOURCE_RS);
  dylib.rust_values().set_crate_type(RustValues::CRATE_DYLIB);  // TODO
  dylib.rust_values().set_crate_root(barlib);
  dylib.rust_values().crate_name() = "mylib";
  dylib.public_deps().push_back(LabelTargetPair(&inside_dylib));
  dylib.private_deps().push_back(LabelTargetPair(&private_inside_dylib));
  dylib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(dylib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&dylib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = dylib\n"
        "output_extension = .so\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.so: rust_dylib ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs "
        "obj/baz/libinside.rlib obj/faz/libprivate_inside.rlib\n"
        "  externs = --extern inside=obj/baz/libinside.rlib "
        "--extern private_inside=obj/faz/libprivate_inside.rlib\n"
        "  rustdeps = -Ldependency=obj/baz -Ldependency=obj/faz\n"
        "  ldflags =\n"
        "  sources = ../../bar/mylib.rs ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target another_dylib(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  another_dylib.set_output_type(Target::SHARED_LIBRARY);
  another_dylib.visibility().SetPublic();
  SourceFile lib("//foo/main.rs");
  another_dylib.sources().push_back(SourceFile("//foo/direct.rs"));
  another_dylib.sources().push_back(lib);
  another_dylib.source_types_used().Set(SourceFile::SOURCE_RS);
  another_dylib.rust_values().set_crate_type(RustValues::CRATE_DYLIB);
  another_dylib.rust_values().set_crate_root(lib);
  another_dylib.rust_values().crate_name() = "direct";
  another_dylib.SetToolchain(setup.toolchain());
  another_dylib.public_deps().push_back(LabelTargetPair(&dylib));
  ASSERT_TRUE(another_dylib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&another_dylib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libdirect.so\n"
        "  externs = --extern direct=obj/foo/libdirect.so "
        "--extern mylib=obj/bar/libmylib.so "
        "--extern inside=obj/baz/libinside.rlib\n"
        "  rustdeps = -Ldependency=obj/foo -Ldependency=obj/bar "
        "-Ldependency=obj/baz -Ldependency=obj/faz\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RlibDepsAcrossGroups) {
  Err err;
  TestWithScope setup;

  Target procmacro(setup.settings(), Label(SourceDir("//bar/"), "mymacro"));
  procmacro.set_output_type(Target::RUST_PROC_MACRO);
  procmacro.visibility().SetPublic();
  SourceFile barproc("//bar/lib.rs");
  procmacro.sources().push_back(SourceFile("//bar/mylib.rs"));
  procmacro.sources().push_back(barproc);
  procmacro.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacro.rust_values().set_crate_root(barproc);
  procmacro.rust_values().crate_name() = "mymacro";
  procmacro.rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
  procmacro.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacro.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&procmacro, out);
    writer.Run();

    const char expected[] =
        "crate_name = mymacro\n"
        "crate_type = proc-macro\n"
        "output_extension = .so\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmymacro\n"
        "\n"
        "build obj/bar/libmymacro.so: rust_macro ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../bar/mylib.rs ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target group(setup.settings(), Label(SourceDir("//baz/"), "group"));
  group.set_output_type(Target::GROUP);
  group.visibility().SetPublic();
  group.public_deps().push_back(LabelTargetPair(&procmacro));
  group.SetToolchain(setup.toolchain());
  ASSERT_TRUE(group.OnResolved(&err));

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  rlib.public_deps().push_back(LabelTargetPair(&group));
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rust_rlib ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs obj/bar/libmymacro.so || "
        "obj/baz/group.stamp\n"
        "  externs = --extern mymacro=obj/bar/libmymacro.so\n"
        "  rustdeps = -Ldependency=obj/bar\n"
        "  ldflags =\n"
        "  sources = ../../bar/mylib.rs ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | "
        "../../foo/source.rs ../../foo/main.rs obj/bar/libmylib.rlib || "
        "obj/baz/group.stamp\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib "
        "--extern mymacro=obj/bar/libmymacro.so\n"
        "  rustdeps = -Ldependency=obj/bar\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RenamedDeps) {
  Err err;
  TestWithScope setup;

  Target transitive(setup.settings(), Label(SourceDir("//faz/"), "transitive"));
  transitive.set_output_type(Target::RUST_LIBRARY);
  transitive.visibility().SetPublic();
  SourceFile transitive_lib("//faz/transitive/lib.rs");
  transitive.sources().push_back(SourceFile("//faz/transitive/transitive.rs"));
  transitive.sources().push_back(transitive_lib);
  transitive.source_types_used().Set(SourceFile::SOURCE_RS);
  transitive.rust_values().set_crate_root(transitive_lib);
  transitive.rust_values().crate_name() = "transitive";
  transitive.SetToolchain(setup.toolchain());
  ASSERT_TRUE(transitive.OnResolved(&err));

  Target rlib(setup.settings(), Label(SourceDir("//baz/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//baz/bar/lib.rs");
  rlib.sources().push_back(SourceFile("//baz/bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  rlib.public_deps().push_back(LabelTargetPair(&transitive));
  ASSERT_TRUE(rlib.OnResolved(&err));

  Target direct(setup.settings(), Label(SourceDir("//bar/"), "direct"));
  direct.set_output_type(Target::RUST_LIBRARY);
  direct.visibility().SetPublic();
  SourceFile direct_lib("//bar/direct/lib.rs");
  direct.sources().push_back(SourceFile("//bar/direct/direct.rs"));
  direct.sources().push_back(direct_lib);
  direct.source_types_used().Set(SourceFile::SOURCE_RS);
  direct.rust_values().set_crate_root(direct_lib);
  direct.rust_values().crate_name() = "direct";
  direct.SetToolchain(setup.toolchain());
  ASSERT_TRUE(direct.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  // A direct dependency is renamed.
  target.rust_values().aliased_deps()[direct.label()] = "direct_renamed";
  // A transitive public dependency, through `rlib`, is renamed.
  target.rust_values().aliased_deps()[transitive.label()] =
      "transitive_renamed";
  target.private_deps().push_back(LabelTargetPair(&direct));
  target.private_deps().push_back(LabelTargetPair(&rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libdirect.rlib obj/baz/libmylib.rlib\n"
        "  externs = --extern direct_renamed=obj/bar/libdirect.rlib "
        "--extern mylib=obj/baz/libmylib.rlib "
        "--extern transitive_renamed=obj/faz/libtransitive.rlib\n"
        "  rustdeps = -Ldependency=obj/bar -Ldependency=obj/baz "
        "-Ldependency=obj/faz\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, NonRustDeps) {
  Err err;
  TestWithScope setup;

  Target staticlib(setup.settings(), Label(SourceDir("//foo/"), "static"));
  staticlib.set_output_type(Target::STATIC_LIBRARY);
  staticlib.visibility().SetPublic();
  staticlib.sources().push_back(SourceFile("//foo/static.cpp"));
  staticlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  staticlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(staticlib.OnResolved(&err));

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  Target sharedlib(setup.settings(), Label(SourceDir("//foo/"), "shared"));
  sharedlib.set_output_type(Target::SHARED_LIBRARY);
  sharedlib.visibility().SetPublic();
  sharedlib.sources().push_back(SourceFile("//foo/static.cpp"));
  sharedlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  sharedlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(sharedlib.OnResolved(&err));

  Target csourceset(setup.settings(), Label(SourceDir("//baz/"), "sourceset"));
  csourceset.set_output_type(Target::SOURCE_SET);
  csourceset.visibility().SetPublic();
  csourceset.sources().push_back(SourceFile("//baz/csourceset.cpp"));
  csourceset.source_types_used().Set(SourceFile::SOURCE_CPP);
  csourceset.SetToolchain(setup.toolchain());
  ASSERT_TRUE(csourceset.OnResolved(&err));

  Toolchain toolchain_with_toc(
      setup.settings(), Label(SourceDir("//toolchain_with_toc/"), "with_toc"));
  TestWithScope::SetupToolchain(&toolchain_with_toc, true);
  Target sharedlib_with_toc(setup.settings(),
                            Label(SourceDir("//foo/"), "shared_with_toc"));
  sharedlib_with_toc.set_output_type(Target::SHARED_LIBRARY);
  sharedlib_with_toc.visibility().SetPublic();
  sharedlib_with_toc.sources().push_back(SourceFile("//foo/static.cpp"));
  sharedlib_with_toc.source_types_used().Set(SourceFile::SOURCE_CPP);
  sharedlib_with_toc.SetToolchain(&toolchain_with_toc);
  ASSERT_TRUE(sharedlib_with_toc.OnResolved(&err));

  Target nonrust(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  nonrust.set_output_type(Target::EXECUTABLE);
  nonrust.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  nonrust.sources().push_back(SourceFile("//foo/source.rs"));
  nonrust.sources().push_back(main);
  nonrust.source_types_used().Set(SourceFile::SOURCE_RS);
  nonrust.rust_values().set_crate_root(main);
  nonrust.rust_values().crate_name() = "foo_bar";
  nonrust.private_deps().push_back(LabelTargetPair(&rlib));
  nonrust.private_deps().push_back(LabelTargetPair(&staticlib));
  nonrust.private_deps().push_back(LabelTargetPair(&sharedlib));
  nonrust.private_deps().push_back(LabelTargetPair(&csourceset));
  nonrust.private_deps().push_back(LabelTargetPair(&sharedlib_with_toc));
  nonrust.SetToolchain(setup.toolchain());
  ASSERT_TRUE(nonrust.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&nonrust, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/baz/sourceset.csourceset.o "
        "obj/bar/libmylib.rlib "
        "obj/foo/libstatic.a ./libshared.so ./libshared_with_toc.so.TOC "
        "|| obj/baz/sourceset.stamp\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar "
        "-Lnative=obj/baz -Lnative=obj/foo -Lnative=. "
        "-Clink-arg=-Bdynamic -Clink-arg=obj/baz/sourceset.csourceset.o "
        "-Clink-arg=obj/foo/libstatic.a -Clink-arg=./libshared.so "
        "-Clink-arg=./libshared_with_toc.so\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target nonrust_only(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  nonrust_only.set_output_type(Target::EXECUTABLE);
  nonrust_only.visibility().SetPublic();
  nonrust_only.sources().push_back(SourceFile("//foo/source.rs"));
  nonrust_only.sources().push_back(main);
  nonrust_only.source_types_used().Set(SourceFile::SOURCE_RS);
  nonrust_only.rust_values().set_crate_root(main);
  nonrust_only.rust_values().crate_name() = "foo_bar";
  nonrust_only.private_deps().push_back(LabelTargetPair(&staticlib));
  nonrust_only.SetToolchain(setup.toolchain());
  ASSERT_TRUE(nonrust_only.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&nonrust_only, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libstatic.a\n"
        "  externs =\n"
        "  rustdeps = -Lnative=obj/foo -Clink-arg=-Bdynamic "
        "-Clink-arg=obj/foo/libstatic.a\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target rstaticlib(setup.settings(), Label(SourceDir("//baz/"), "baz"));
  rstaticlib.set_output_type(Target::STATIC_LIBRARY);
  rstaticlib.visibility().SetPublic();
  SourceFile bazlib("//baz/lib.rs");
  rstaticlib.sources().push_back(bazlib);
  rstaticlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rstaticlib.rust_values().set_crate_root(bazlib);
  rstaticlib.rust_values().crate_name() = "baz";
  rstaticlib.private_deps().push_back(LabelTargetPair(&staticlib));
  rstaticlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rstaticlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rstaticlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = baz\n"
        "crate_type = staticlib\n"
        "output_extension = .a\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/baz\n"
        "target_output_name = libbaz\n"
        "\n"
        "build obj/baz/libbaz.a: rust_staticlib ../../baz/lib.rs | "
        "../../baz/lib.rs "
        "obj/foo/libstatic.a\n"
        "  externs =\n"
        "  rustdeps = -Lnative=obj/foo -Clink-arg=-Bdynamic "
        "-Clink-arg=obj/foo/libstatic.a\n"
        "  ldflags =\n"
        "  sources = ../../baz/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustOutputExtensionAndDir) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input3.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.set_output_extension(std::string("exe"));
  target.set_output_dir(SourceDir("//out/Debug/foo/"));
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = .exe\n"
        "output_dir = foo\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar.exe: rust_bin ../../foo/main.rs | ../../foo/input3.rs "
        "../../foo/main.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../foo/input3.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, LibsAndLibDirs) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.set_output_dir(SourceDir("//out/Debug/foo/"));
  target.config_values().libs().push_back(LibFile("quux"));
  target.config_values().lib_dirs().push_back(SourceDir("//baz/"));
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = foo\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/input.rs "
        "../../foo/main.rs\n"
        "  externs =\n"
        "  rustdeps = -Lnative=../../baz -lquux\n"
        "  ldflags =\n"
        "  sources = ../../foo/input.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

// Test that neither public nor private rust dependencies of a proc-macro are
// transitively acquired as accessible dependencies by users of the macro. But
// the macro itself is listed as an accessible dependency (via --extern).
TEST_F(NinjaRustBinaryTargetWriterTest, RustProcMacro) {
  Err err;
  TestWithScope setup;

  Target procmacropublicdep(
      setup.settings(), Label(SourceDir("//baz/public/"), "mymacropublicdep"));
  procmacropublicdep.set_output_type(Target::RUST_LIBRARY);
  procmacropublicdep.visibility().SetPublic();
  SourceFile publicbazlib("//baz/public/lib.rs");
  procmacropublicdep.sources().push_back(SourceFile("//baz/public/mylib.rs"));
  procmacropublicdep.sources().push_back(publicbazlib);
  procmacropublicdep.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacropublicdep.rust_values().set_crate_root(publicbazlib);
  procmacropublicdep.rust_values().crate_name() = "publicdep";
  procmacropublicdep.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacropublicdep.OnResolved(&err));

  Target procmacroprivatedep(
      setup.settings(),
      Label(SourceDir("//baz/private/"), "mymacroprivatedep"));
  procmacroprivatedep.set_output_type(Target::RUST_LIBRARY);
  procmacroprivatedep.visibility().SetPublic();
  SourceFile privatebazlib("//baz/private/lib.rs");
  procmacroprivatedep.sources().push_back(SourceFile("//baz/private/mylib.rs"));
  procmacroprivatedep.sources().push_back(privatebazlib);
  procmacroprivatedep.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacroprivatedep.rust_values().set_crate_root(privatebazlib);
  procmacroprivatedep.rust_values().crate_name() = "privatedep";
  procmacroprivatedep.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacroprivatedep.OnResolved(&err));

  Target procmacro(setup.settings(), Label(SourceDir("//bar/"), "mymacro"));
  procmacro.set_output_type(Target::RUST_PROC_MACRO);
  procmacro.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  procmacro.sources().push_back(SourceFile("//bar/mylib.rs"));
  procmacro.sources().push_back(barlib);
  procmacro.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacro.rust_values().set_crate_root(barlib);
  procmacro.rust_values().crate_name() = "mymacro";
  procmacro.rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
  // Add a dependency to the procmacro so we can be sure its output
  // directory is not propagated downstream beyond the proc macro.
  procmacro.public_deps().push_back(LabelTargetPair(&procmacropublicdep));
  procmacro.private_deps().push_back(LabelTargetPair(&procmacroprivatedep));
  procmacro.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacro.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&procmacro, out);
    writer.Run();

    const char expected[] =
        "crate_name = mymacro\n"
        "crate_type = proc-macro\n"
        "output_extension = .so\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmymacro\n"
        "\n"
        "build obj/bar/libmymacro.so: rust_macro ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs "
        "obj/baz/public/libmymacropublicdep.rlib "
        "obj/baz/private/libmymacroprivatedep.rlib\n"
        "  externs = "
        "--extern publicdep=obj/baz/public/libmymacropublicdep.rlib "
        "--extern privatedep=obj/baz/private/libmymacroprivatedep.rlib\n"
        "  rustdeps = -Ldependency=obj/baz/public "
        "-Ldependency=obj/baz/private\n"
        "  ldflags =\n"
        "  sources = ../../bar/mylib.rs ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&procmacro));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmymacro.so\n"
        "  externs = --extern mymacro=obj/bar/libmymacro.so\n"
        "  rustdeps = -Ldependency=obj/bar\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, GroupDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_extension = .rlib\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rust_rlib ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../bar/mylib.rs ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target group(setup.settings(), Label(SourceDir("//baz/"), "group"));
  group.set_output_type(Target::GROUP);
  group.visibility().SetPublic();
  group.public_deps().push_back(LabelTargetPair(&rlib));
  group.SetToolchain(setup.toolchain());
  ASSERT_TRUE(group.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&group));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmylib.rlib || obj/baz/group.stamp\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, Externs) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";

  const char* lib = "lib1";
  target.config_values().externs().push_back(
      std::pair(lib, LibFile(SourceFile("//foo/lib1.rlib"))));
  lib = "lib2";
  target.config_values().externs().push_back(
      std::pair(lib, LibFile("lib2.rlib")));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs ../../foo/lib1.rlib\n"
        "  externs = --extern lib1=../../foo/lib1.rlib --extern "
        "lib2=lib2.rlib\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, Inputs) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.config_values().inputs().push_back(SourceFile("//foo/config.json"));
  target.config_values().inputs().push_back(SourceFile("//foo/template.h"));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "build obj/foo/bar.inputs.stamp: stamp ../../foo/config.json "
        "../../foo/template.h\n"
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs ../../foo/config.json ../../foo/template.h "
        "|| obj/foo/bar.inputs.stamp\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs "
        "../../foo/config.json ../../foo/template.h\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, CdylibDeps) {
  Err err;
  TestWithScope setup;
  Target cdylib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  cdylib.set_output_type(Target::SHARED_LIBRARY);
  cdylib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  cdylib.sources().push_back(barlib);
  cdylib.source_types_used().Set(SourceFile::SOURCE_RS);
  cdylib.rust_values().set_crate_type(RustValues::CRATE_CDYLIB);
  cdylib.rust_values().set_crate_root(barlib);
  cdylib.rust_values().crate_name() = "mylib";
  cdylib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(cdylib.OnResolved(&err));
  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&cdylib, out);
    writer.Run();
    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = cdylib\n"
        "output_extension = .so\n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = libmylib\n"
        "\n"
        "build obj/bar/libmylib.so: rust_cdylib ../../bar/lib.rs | "
        "../../bar/lib.rs\n"
        "  externs =\n"
        "  rustdeps =\n"
        "  ldflags =\n"
        "  sources = ../../bar/lib.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&cdylib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));
  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmylib.so\n"
        "  externs =\n"
        "  rustdeps = -Lnative=obj/bar -Clink-arg=-Bdynamic "
        "-Clink-arg=obj/bar/libmylib.so\n"
        "  ldflags =\n"
        "  sources = ../../foo/source.rs ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, TransitivePublicNonRustDeps) {
  Err err;
  TestWithScope setup;

  // This test verifies that the Rust binary "target" links against this lib.
  Target implicitlib(setup.settings(), Label(SourceDir("//foo/"), "implicit"));
  implicitlib.set_output_type(Target::SHARED_LIBRARY);
  implicitlib.visibility().SetPublic();
  implicitlib.sources().push_back(SourceFile("//foo/implicit.cpp"));
  implicitlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  implicitlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(implicitlib.OnResolved(&err));

  Target sharedlib(setup.settings(), Label(SourceDir("//foo/"), "shared"));
  sharedlib.set_output_type(Target::SHARED_LIBRARY);
  sharedlib.visibility().SetPublic();
  sharedlib.sources().push_back(SourceFile("//foo/shared.cpp"));
  sharedlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  sharedlib.SetToolchain(setup.toolchain());
  sharedlib.public_deps().push_back(LabelTargetPair(&implicitlib));
  ASSERT_TRUE(sharedlib.OnResolved(&err));

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.SetToolchain(setup.toolchain());
  rlib.private_deps().push_back(LabelTargetPair(&sharedlib));
  ASSERT_TRUE(rlib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.private_deps().push_back(LabelTargetPair(&rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_extension = \n"
        "output_dir = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build ./foo_bar: rust_bin ../../foo/main.rs | ../../foo/main.rs "
        "obj/bar/libmylib.rlib ./libshared.so ./libimplicit.so\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar -Lnative=. -Clink-arg=-Bdynamic "
        "-Clink-arg=./libshared.so -Clink-arg=./libimplicit.so\n"
        "  ldflags =\n"
        "  sources = ../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}
