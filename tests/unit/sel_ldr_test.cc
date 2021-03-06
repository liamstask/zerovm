/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * update(d'b): i think this test can be removed (or should be rewritten)
 */

#include "src/loader/sel_ldr.h"
#include "gtest/gtest.h"
#include "src/loader/sel_ldr_x86.h"

//
// There are several problems in how these tests are set up.
//
// 1. NaCl modules such as the Log module are supposed to be
// initialized at process startup and finalized at shutdown.  In
// particular, there should not be any threads other than the main
// thread running when the Log module initializes, since the verbosity
// level is set then -- and thereafter it is assumed to be invariant
// and read without acquring locks.  If any threads are left running
// (e.g., NaClApp internal service threads), then race detectors would
// legitimately report an error which is inappropriate because the
// test is ignoring the API contract.
//
// 2. NaClApp objects, while they don't have a Dtor, are expected to
// have a lifetime equal to that of the process that contain them.  In
// particular, when the untrusted thread invokes the exit syscall, it
// expects to be able to use _exit to exit, killing all other
// untrusted threads as a side effect.  Furthermore, once a NaClApp
// object is initialized and NaClAppLaunchServiceThreads invoked,
// system service threads are running holding references to the
// NaClApp object.  If the NaClApp object goes out of scope or is
// otherwise destroyed and its memory freed, then these system thread
// may access memory that is no longer valid.  Tests cannot readily be
// written to cleanly exercise the state space of a NaClApp after
// NaClAppLaunchServiceThreads unless the test process exits---thereby
// killing the service threads as a side-effect---when each individual
// test is complete.
//
// These tests do not invoke NaClAppLaunchServiceThreads, so there
// should be no service threads left running between tests.

class SelLdrTest : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();
};

void SelLdrTest::SetUp() {
}

void SelLdrTest::TearDown() {
}

// set, get, setavail operations on the descriptor table
// todo: test is almost useless and should be removed or updated
TEST_F(SelLdrTest, DescTable) {
  struct NaClApp app = {0};

  NaClAppCtor(&app);
  ASSERT_EQ(NACL_MAX_ADDR_BITS, app.addr_bits);
  ASSERT_EQ((uintptr_t)NACL_DEFAULT_STACK_MAX, app.stack_size);
  ASSERT_EQ(0u, app.mem_start);
  ASSERT_EQ(0u, app.dispatch_thunk);
  ASSERT_EQ(0u, app.static_text_end);
  ASSERT_EQ(0u, app.dynamic_text_start);
  ASSERT_EQ(0u, app.dynamic_text_end);
  ASSERT_EQ(0u, app.rodata_start);
  ASSERT_EQ(0u, app.data_start);
  ASSERT_EQ(0u, app.data_end);
  ASSERT_EQ(0u, app.initial_entry_pt);
  ASSERT_EQ(0u, app.user_entry_pt);
}
