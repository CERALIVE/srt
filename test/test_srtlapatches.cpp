/*
 * SRT - Secure, Reliable, Transport
 *
 * CERALIVE compatibility option tests.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <gtest/gtest.h>

#include "test_env.h"

#include "socketconfig.h"
#include "srt.h"

// SRTO_SRTLAPATCHES is a compatibility shim over SRTO_REORDERFREEZE (bool) and
// SRTO_PERIODICNAKGATE (tri-state): setting it non-zero turns both on with the
// D10 default gate, setting it zero clears both. Only 0/non-zero are meaningful.
// Each underlying option may still be written afterwards; the last write wins.

namespace {

bool getSrtlaPatches(SRTSOCKET sock)
{
    bool val = false;
    int  len = sizeof val;
    EXPECT_EQ(srt_getsockopt(sock, 0, SRTO_SRTLAPATCHES, &val, &len), SRT_SUCCESS);
    return val;
}

bool getReorderFreeze(SRTSOCKET sock)
{
    bool val = false;
    int  len = sizeof val;
    EXPECT_EQ(srt_getsockopt(sock, 0, SRTO_REORDERFREEZE, &val, &len), SRT_SUCCESS);
    return val;
}

int getPeriodicNakGate(SRTSOCKET sock)
{
    int val = -1;
    int len = sizeof val;
    EXPECT_EQ(srt_getsockopt(sock, 0, SRTO_PERIODICNAKGATE, &val, &len), SRT_SUCCESS);
    return val;
}

} // namespace

TEST(SrtlaPatchesOption, NonZeroEnablesReorderFreezeAndNakGate)
{
    srt::TestInit srtinit;

    MAKE_UNIQUE_SOCK(sock, "srtlapatches non-zero", srt_create_socket());
    ASSERT_NE(sock.ref(), SRT_INVALID_SOCK);

    EXPECT_FALSE(getSrtlaPatches(sock.ref())) << "SRTO_SRTLAPATCHES must default to off";

    const int one = 1;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_SRTLAPATCHES, &one, sizeof one), SRT_SUCCESS);

    EXPECT_TRUE(getSrtlaPatches(sock.ref())) << "SRTO_SRTLAPATCHES=1 must read back as set";
    EXPECT_TRUE(getReorderFreeze(sock.ref())) << "SRTO_SRTLAPATCHES=1 must enable SRTO_REORDERFREEZE";
    EXPECT_EQ(getPeriodicNakGate(sock.ref()), SRTLA_PATCHES_DEFAULT_NAKGATE)
        << "SRTO_SRTLAPATCHES=1 must install the D10 default gate";

    // Only 0/non-zero are meaningful: any other non-zero int is accepted as "on".
    const int two = 2;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_SRTLAPATCHES, &two, sizeof two), SRT_SUCCESS);
    EXPECT_EQ(getPeriodicNakGate(sock.ref()), SRTLA_PATCHES_DEFAULT_NAKGATE);
    EXPECT_TRUE(getReorderFreeze(sock.ref()));
}

TEST(SrtlaPatchesOption, ZeroClearsBothUnderlyingOptions)
{
    srt::TestInit srtinit;

    MAKE_UNIQUE_SOCK(sock, "srtlapatches zero", srt_create_socket());
    ASSERT_NE(sock.ref(), SRT_INVALID_SOCK);

    const int one = 1;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_SRTLAPATCHES, &one, sizeof one), SRT_SUCCESS);
    ASSERT_TRUE(getSrtlaPatches(sock.ref()));

    const int zero = 0;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_SRTLAPATCHES, &zero, sizeof zero), SRT_SUCCESS);

    EXPECT_FALSE(getSrtlaPatches(sock.ref()));
    EXPECT_FALSE(getReorderFreeze(sock.ref())) << "SRTO_SRTLAPATCHES=0 must clear SRTO_REORDERFREEZE";
    EXPECT_EQ(getPeriodicNakGate(sock.ref()), 0) << "SRTO_SRTLAPATCHES=0 must clear SRTO_PERIODICNAKGATE";
}

TEST(SrtlaPatchesOption, ExplicitPeriodicNakGateAfterSrtlaPatchesWins)
{
    srt::TestInit srtinit;

    MAKE_UNIQUE_SOCK(sock, "srtlapatches override", srt_create_socket());
    ASSERT_NE(sock.ref(), SRT_INVALID_SOCK);

    const int one = 1;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_SRTLAPATCHES, &one, sizeof one), SRT_SUCCESS);
    ASSERT_EQ(getPeriodicNakGate(sock.ref()), SRTLA_PATCHES_DEFAULT_NAKGATE);

    const int gate_override = 1;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_PERIODICNAKGATE, &gate_override, sizeof gate_override), SRT_SUCCESS);

    EXPECT_EQ(getPeriodicNakGate(sock.ref()), 1)
        << "an explicit SRTO_PERIODICNAKGATE write after SRTO_SRTLAPATCHES must win";
    EXPECT_TRUE(getSrtlaPatches(sock.ref()))
        << "the compat getter is a conjunction: freeze on AND gate non-zero";

    const int gate_off = 0;
    ASSERT_EQ(srt_setsockopt(sock.ref(), 0, SRTO_PERIODICNAKGATE, &gate_off, sizeof gate_off), SRT_SUCCESS);
    EXPECT_FALSE(getSrtlaPatches(sock.ref()))
        << "a zero gate makes the conjunction read false even with reorder-freeze still on";
}
