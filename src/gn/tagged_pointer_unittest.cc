// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/tagged_pointer.h"
#include "util/test/test.h"

struct Point {
  double x;
  double y;
};

TEST(TaggedPointer, Creation) {
  TaggedPointer<Point, 2> ptr;

  EXPECT_FALSE(ptr.ptr());
  EXPECT_EQ(0u, ptr.tag());

  Point point1 = {1., 2.};
  TaggedPointer<Point, 2> ptr2(&point1, 2);
  EXPECT_EQ(&point1, ptr2.ptr());
  EXPECT_EQ(2u, ptr2.tag());
}
