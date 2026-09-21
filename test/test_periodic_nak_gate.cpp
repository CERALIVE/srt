#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "test_env.h"

#include "srt.h"
#include "api.h"
#include "core.h"
#include "list.h"
#include "packet.h"

using namespace srt;
using namespace srt::sync;

namespace srt {
    // Friend wrapper for unit tests that drive the private periodic-NAK decision.
    // Declared as a friend in core.h.
    class TestMockPeriodicNakGate
    {
    public:
        CUDT* core;

        TestMockPeriodicNakGate(): core(NULL) {}

        int checkNAKTimer(const steady_clock::time_point& t) { return core->checkNAKTimer(t); }

        void buildFilteredLossReport(std::vector<int32_t>& out) { core->buildFilteredLossReport((out)); }

        void insertLoss(int32_t lo, int32_t hi)
        {
            ScopedLock lk(core->m_RcvLossLock);
            core->m_pRcvLossList->insert(lo, hi);
        }

        void addFreshLoss(int32_t lo, int32_t hi, int ttl)
        {
            ScopedLock lk(core->m_RcvLossLock);
            core->m_FreshLoss.push_back(CRcvFreshLoss(lo, hi, ttl));
        }

        int lossLength()
        {
            ScopedLock lk(core->m_RcvLossLock);
            return core->m_pRcvLossList->getLossLength();
        }

        uint32_t sentNakTotal() const { return core->m_stats.rcvr.sentNak.total.count(); }

        void setGate(int v) { core->m_config.iPeriodicNakGate = v; }

        void setNextNakTime(const steady_clock::time_point& t) { core->m_tsNextNAKTime.store(t); }
        steady_clock::time_point nextNakTime() const { return core->m_tsNextNAKTime.load(); }

        void detachFromQueues() { core->m_bConnected = false; }
        void reattachToQueues() { core->m_bConnected = true; }
    };
}

namespace {

const int  kIntervals   = 5;
const char kListenHost[] = "localhost";
const int  kListenPort  = 5565;

// Encoded form of a multi-sequence loss range as produced by CUDT::addLossRecord.
std::vector<int32_t> lossRange(int32_t lo, int32_t hi)
{
    std::vector<int32_t> v;
    if (lo == hi)
    {
        v.push_back(lo);
        return v;
    }
    v.push_back(lo | LOSSDATA_SEQNO_RANGE_FIRST);
    v.push_back(hi);
    return v;
}

void appendRange(std::vector<int32_t>& v, int32_t lo, int32_t hi)
{
    const std::vector<int32_t> r = lossRange(lo, hi);
    v.insert(v.end(), r.begin(), r.end());
}

} // namespace

class PeriodicNakGate: public srt::Test
{
public:
    SRTSOCKET caller   = SRT_INVALID_SOCK;
    SRTSOCKET listener = SRT_INVALID_SOCK;
    SRTSOCKET accepted = SRT_INVALID_SOCK;
    CUDTSocket* pcaller = NULL;
    TestMockPeriodicNakGate mock;

    static void swipe(SRTSOCKET& sockid)
    {
        if (sockid == SRT_INVALID_SOCK)
            return;

        EXPECT_NE(srt_close(sockid), SRT_ERROR);
        sockid = SRT_INVALID_SOCK;
    }

    void setup() override
    {
        caller = CUDT::uglobal().newSocket(&pcaller);
        ASSERT_NE(caller, SRT_INVALID_SOCK);
        mock.core = &pcaller->core();

        ASSERT_NE(listener = srt_create_socket(), SRT_INVALID_SOCK);

        srt::sockaddr_any sa = srt::CreateAddr(kListenHost, kListenPort, AF_INET);
        ASSERT_NE(srt_bind(listener, sa.get(), sa.size()), SRT_ERROR);
        ASSERT_NE(srt_listen(listener, 1), SRT_ERROR);

        std::thread spawned_connect([this, &sa] { EXPECT_NE(srt_connect(caller, sa.get(), sa.size()), SRT_ERROR); });

        accepted = srt_accept(listener, NULL, 0);
        spawned_connect.join();
        ASSERT_NE(accepted, SRT_ERROR);
    }

    // Both CRcvQueue paths that call CUDT::checkTimers() refuse a socket whose
    // m_bConnected is false, so clearing it makes this socket's NAK timer fire
    // only when the test fires it. Without that, the queue worker's own ~10ms
    // checkTimers() tick would race every count taken here.
    void detachFromQueues()
    {
        mock.detachFromQueues();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Drives the periodic-NAK decision exactly `count` times, forcing the NAK
    // timer to be due before each call, and returns the number of LOSSREPORT
    // packets the send path actually emitted.
    uint32_t runDueNakTimers(int count)
    {
        const uint32_t before = mock.sentNakTotal();
        for (int i = 0; i < count; ++i)
        {
            const steady_clock::time_point now = steady_clock::now();
            mock.setNextNakTime(now - milliseconds_from(1000));
            mock.checkNAKTimer(now);
            EXPECT_GT(mock.nextNakTime(), now) << "the NAK timer must advance on every due tick";
        }
        return mock.sentNakTotal() - before;
    }

    void teardown() override
    {
        if (mock.core != NULL)
            mock.reattachToQueues();
        swipe(caller);
        swipe(accepted);
        swipe(listener);
    }
};

TEST(PeriodicNakGateOption, SetGetRoundTripAndRejection)
{
    srt::TestInit srtinit;

    MAKE_UNIQUE_SOCK(sock, "periodicnakgate round trip", srt_create_socket());
    ASSERT_NE(sock.ref(), SRT_INVALID_SOCK);

    int      val = -1;
    int      len = sizeof val;
    ASSERT_EQ(srt_getsockopt(sock, 0, SRTO_PERIODICNAKGATE, &val, &len), SRT_SUCCESS);
    EXPECT_EQ(val, 0) << "SRTO_PERIODICNAKGATE must default to off";

    for (int accepted_value = 0; accepted_value <= 2; ++accepted_value)
    {
        ASSERT_EQ(srt_setsockopt(sock, 0, SRTO_PERIODICNAKGATE, &accepted_value, sizeof accepted_value), SRT_SUCCESS)
            << "SRTO_PERIODICNAKGATE must accept " << accepted_value;

        val = -1;
        len = sizeof val;
        ASSERT_EQ(srt_getsockopt(sock, 0, SRTO_PERIODICNAKGATE, &val, &len), SRT_SUCCESS);
        EXPECT_EQ(val, accepted_value);
        EXPECT_EQ(len, (int) sizeof(int));
    }

    const int three = 3;
    EXPECT_EQ(srt_setsockopt(sock, 0, SRTO_PERIODICNAKGATE, &three, sizeof three), SRT_ERROR);
    EXPECT_EQ(srt_getlasterror(NULL), SRT_EINVPARAM);

    const int negative = -1;
    EXPECT_EQ(srt_setsockopt(sock, 0, SRTO_PERIODICNAKGATE, &negative, sizeof negative), SRT_ERROR);
    EXPECT_EQ(srt_getlasterror(NULL), SRT_EINVPARAM);

    val = -1;
    len = sizeof val;
    ASSERT_EQ(srt_getsockopt(sock, 0, SRTO_PERIODICNAKGATE, &val, &len), SRT_SUCCESS);
    EXPECT_EQ(val, 2) << "a rejected value must leave the last accepted one in place";
}

// Arm 2 is the upstream-exact suppress semantics: with a non-empty receiver loss
// list and a due NAK timer, the periodic site emits no LOSSREPORT at all, while
// the NAK timer still advances on every tick. Arm 0 (the default) is the control.
TEST_F(PeriodicNakGate, SuppressArmEmitsNoLossReports)
{
    detachFromQueues();

    mock.insertLoss(1000, 1010);
    ASSERT_GT(mock.lossLength(), 0);

    mock.setGate(2);
    EXPECT_EQ(runDueNakTimers(kIntervals), 0u)
        << "SRTO_PERIODICNAKGATE=2 must emit no periodic LOSSREPORT";

    ASSERT_GT(mock.lossLength(), 0) << "the loss list must still be non-empty for the control arm";

    mock.setGate(0);
    EXPECT_EQ(runDueNakTimers(kIntervals), (uint32_t) kIntervals)
        << "SRTO_PERIODICNAKGATE=0 must emit one periodic LOSSREPORT per due tick";
}

// Arm 1 subtracts the ranges that are still inside their reorder TTL and reports
// what is left, so a partially-reorderable loss list still produces a LOSSREPORT
// while a fully-reorderable one produces none.
TEST_F(PeriodicNakGate, FilterArmSubtractsFreshLossAndSendsTheRest)
{
    detachFromQueues();

    mock.insertLoss(1000, 1010);
    ASSERT_GT(mock.lossLength(), 0);

    std::vector<int32_t> unfiltered;
    mock.buildFilteredLossReport(unfiltered);
    std::vector<int32_t> whole_range;
    appendRange(whole_range, 1000, 1010);
    EXPECT_EQ(unfiltered, whole_range) << "with no fresh loss the whole range must be reported";

    mock.addFreshLoss(1003, 1005, 20);

    std::vector<int32_t> filtered;
    mock.buildFilteredLossReport(filtered);

    std::vector<int32_t> expected;
    appendRange(expected, 1000, 1002);
    appendRange(expected, 1006, 1010);
    EXPECT_EQ(filtered, expected) << "the still-reorderable range must be subtracted, the rest kept";

    mock.setGate(1);
    EXPECT_EQ(runDueNakTimers(kIntervals), (uint32_t) kIntervals)
        << "a partially-filtered loss list must still be reported";

    mock.addFreshLoss(1000, 1010, 20);

    std::vector<int32_t> all_fresh;
    mock.buildFilteredLossReport(all_fresh);
    EXPECT_TRUE(all_fresh.empty()) << "a fully-reorderable loss list must filter down to nothing";

    EXPECT_EQ(runDueNakTimers(kIntervals), 0u)
        << "nothing left after filtering means no LOSSREPORT is sent";
}
