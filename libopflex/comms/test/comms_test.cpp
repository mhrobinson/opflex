/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Test suite for Processor class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <opflex/comms/comms-internal.hpp>
#include <opflex/logging/internal/logging.hpp>
#include <cstdlib>
#include <boost/test/unit_test_log.hpp>

#include <dlfcn.h>

using opflex::comms::comms_passive_listener;
using opflex::comms::comms_active_connection;
using namespace opflex::comms::internal;

BOOST_AUTO_TEST_SUITE(asynchronous_sockets)

struct CommsTests {
    CommsTests() {
        LOG(INFO) << "global setup\n";

        boost::unit_test::unit_test_log_t::instance().set_threshold_level(::boost::unit_test::log_successful_tests);
    }
    ~CommsTests() {
        LOG(INFO) << "global teardown\n";
    }
};

BOOST_GLOBAL_FIXTURE( CommsTests );

/**
 * A fixture for communications tests
 */
class CommsFixture {
  private:
    uv_idle_t  * idler;
    uv_timer_t * timer;

  protected:
    CommsFixture()
        :
            idler((typeof(idler))malloc(sizeof(*idler))),
            timer((typeof(timer))malloc(sizeof(*timer))) {

        LOG(INFO) << "\n\n\n\n\n\n\n\n";

        LOG(DEBUG) << "idler = " << idler;
        LOG(DEBUG) << "timer = " << timer;
        idler->data = timer->data = this;

        int rc = opflex::comms::initCommunicationLoop(uv_default_loop());

        BOOST_CHECK(!rc);

        for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
            BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                        Peer::LoopData::PeerState(i))
                    ->size(), 0);
        }

        uv_idle_init(uv_default_loop(), idler);
        uv_idle_start(idler, check_peer_db_cb);

        uv_timer_init(uv_default_loop(), timer);
        uv_timer_start(timer, timeout_cb, 1800, 0);
        uv_unref((uv_handle_t*) timer);

    }

    struct PeerDisposer {
        void operator()(Peer *peer)
        {
            LOG(DEBUG) << peer << " destroy() with intent of deleting";
            peer->destroy();
        }
    };

    void cleanup() {

        LOG(DEBUG);

        uv_timer_stop(timer);
        uv_idle_stop(idler);

        for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
            Peer::LoopData::getPeerList(uv_default_loop(),
                        Peer::LoopData::PeerState(i))
                ->clear_and_dispose(PeerDisposer());

        }

        uv_idle_start(idler, wait_for_zero_peers_cb);

    }

    ~CommsFixture() {

        uv_close((uv_handle_t *)timer, (uv_close_cb)free);
        uv_close((uv_handle_t *)idler, (uv_close_cb)free);

        opflex::comms::finiCommunicationLoop(uv_default_loop());

        LOG(INFO) << "\n\n\n\n\n\n\n\n";

    }

    typedef void (*pc)(void);

    static size_t required_final_peers;
    static pc required_post_conditions;
    static bool expect_timeout;

    static size_t count_final_peers() {

        static std::string oldDbgLog;
        std::stringstream dbgLog;
        std::string newDbgLog;

        size_t final_peers = 0;

        size_t m;
        if((m=Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::ONLINE)->size())) {
            final_peers += m;
            dbgLog << " online: " << m;
        }
        if((m=Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::RETRY_TO_CONNECT)->size())) {
            final_peers += m;
            dbgLog << " retry-connecting: " << m;
        }
        if((m=Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::ATTEMPTING_TO_CONNECT)->size())) {
            /* this is not a "final" state, from a test's perspective */
            dbgLog << " attempting: " << m;
        }
        if((m=Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::RETRY_TO_LISTEN)->size())) {
            final_peers += m;
            dbgLog << " retry-listening: " << m;
        }
        if((m=Peer::LoopData::getPeerList(
                    uv_default_loop(),
                    internal::Peer::LoopData::LISTENING)->size())) {
            final_peers += m;
            dbgLog << " listening: " << m;
        }

        dbgLog << " TOTAL FINAL: " << final_peers << "\0";

        newDbgLog = dbgLog.str();

        if (oldDbgLog != newDbgLog) {
            oldDbgLog = newDbgLog;

            LOG(DEBUG) << newDbgLog;
        }

        return final_peers;
    }

    static void wait_for_zero_peers_cb(uv_idle_t * handle) {

        static size_t old_count;
        size_t count = Peer::getCounter();

        if (count) {
            if (old_count != count) {
                LOG(DEBUG) << count << " peers left";
            }
            old_count = count;
            return;
        }
        LOG(DEBUG) << "no peers left";

        uv_idle_stop(handle);
        uv_stop(uv_default_loop());

    }

    static void check_peer_db_cb(uv_idle_t * handle) {

        /* check all easily reachable peers' invariants */
        for (size_t i=0; i < Peer::LoopData::TOTAL_STATES; ++i) {
            Peer::List * pL = Peer::LoopData::getPeerList(uv_default_loop(),
                        Peer::LoopData::PeerState(i));

            for(Peer::List::iterator it = pL->begin(); it != pL->end(); ++it) {
                it->__checkInvariants();
            }
        }

        if (count_final_peers() < required_final_peers) {
            return;
        }

        (*required_post_conditions)();

        reinterpret_cast<CommsFixture*>(handle->data)->cleanup();

    }

    static void timeout_cb(uv_timer_t * handle) {

        LOG(DEBUG);

        if (!expect_timeout) {
            BOOST_CHECK(!"the test has timed out");
        }

        (*required_post_conditions)();

        reinterpret_cast<CommsFixture*>(handle->data)->cleanup();

    }

    void loop_until_final(size_t final_peers, pc post_conditions, bool timeout = false) {

        LOG(DEBUG);

        required_final_peers = final_peers;
        required_post_conditions = post_conditions;
        expect_timeout = timeout;

        uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    }

};

size_t CommsFixture::required_final_peers;
CommsFixture::pc CommsFixture::required_post_conditions;
bool CommsFixture::expect_timeout;

BOOST_FIXTURE_TEST_CASE( test_initialization, CommsFixture ) {

    LOG(DEBUG);

}

void pc_successful_connect(void) {

    LOG(DEBUG);

    /* empty */
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::RETRY_TO_CONNECT)
            ->size(), 0);
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::RETRY_TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::ATTEMPTING_TO_CONNECT)
            ->size(), 0);

    /* non-empty */
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::ONLINE)
            ->size(), 2);
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::LISTENING)
            ->size(), 1);

}

class DoNothingOnConnect {
  public:
    void operator()(opflex::comms::internal::CommunicationPeer * p) {
        LOG(INFO) << "chill out, we just had a" << (
                p->passive_ ? " pass" : "n act"
            ) << "ive connection";
    }
};

opflex::comms::internal::ConnectionHandler doNothingOnConnect = DoNothingOnConnect();

class StartPingingOnConnect {
  public:
    void operator()(opflex::comms::internal::CommunicationPeer * p) {
        p->startKeepAlive();
    }
};

opflex::comms::internal::ConnectionHandler startPingingOnConnect = StartPingingOnConnect();

BOOST_FIXTURE_TEST_CASE( test_ipv4, CommsFixture ) {

    LOG(DEBUG);

    int rc;

    rc = comms_passive_listener(doNothingOnConnect, "127.0.0.1", 65535);

    BOOST_CHECK_EQUAL(rc, 0);

    rc = comms_active_connection(doNothingOnConnect, "localhost", "65535");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(3, pc_successful_connect);

}

BOOST_FIXTURE_TEST_CASE( test_ipv6, CommsFixture ) {

    LOG(DEBUG);

    int rc;

    rc = comms_passive_listener(doNothingOnConnect, "::1", 65534);

    BOOST_CHECK_EQUAL(rc, 0);

    rc = comms_active_connection(doNothingOnConnect, "localhost", "65534");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(3, pc_successful_connect);

}

static void pc_non_existent(void) {

    LOG(DEBUG);

    /* non-empty */
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::RETRY_TO_CONNECT)
            ->size(), 1);

    /* empty */
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::RETRY_TO_LISTEN)
            ->size(), 0);
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::ATTEMPTING_TO_CONNECT)
            ->size(), 0);
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::ONLINE)
            ->size(), 0);
    BOOST_CHECK_EQUAL(Peer::LoopData::getPeerList(uv_default_loop(),
                Peer::LoopData::LISTENING)
            ->size(), 0);

}

BOOST_FIXTURE_TEST_CASE( test_non_existent_host, CommsFixture ) {

    LOG(DEBUG);

    int rc = comms_active_connection(doNothingOnConnect, "non_existent_host.", "65533");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(1, pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( test_non_existent_service, CommsFixture ) {

    LOG(DEBUG);

    int rc = comms_active_connection(doNothingOnConnect, "127.0.0.1", "65533");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(1, pc_non_existent);

}

BOOST_FIXTURE_TEST_CASE( test_keepalive, CommsFixture ) {

    LOG(DEBUG);

    int rc;

    rc = comms_passive_listener(startPingingOnConnect, "::1", 65532);

    BOOST_CHECK_EQUAL(rc, 0);

    rc = comms_active_connection(startPingingOnConnect, "::1", "65532");

    BOOST_CHECK_EQUAL(rc, 0);

    loop_until_final(4, pc_successful_connect, true); // 4 is to cause a timeout

}

BOOST_AUTO_TEST_SUITE_END()
