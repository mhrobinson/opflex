/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Include file for base fixture
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflex/ofcore/OFFramework.h>
#include <modelgbp/metadata/metadata.hpp>

#include "Agent.h"

#pragma once
#ifndef OVSAGENT_TEST_BASEFIXTURE_H
#define OVSAGENT_TEST_BASEFIXTURE_H

namespace ovsagent {

using opflex::ofcore::MockOFFramework;

/**
 * A fixture that adds an object store
 */
class BaseFixture {
public:
    BaseFixture() : agent(framework) {
        agent.start();
    }

    virtual ~BaseFixture() {
        agent.stop();
    }

    MockOFFramework framework;
    Agent agent;
};

// wait for a condition to become true because of an event in another
// thread
#define WAIT_FOR(condition, count)                             \
    {                                                          \
        int _c = 0;                                            \
        while (_c < count) {                                   \
           if (condition) break;                               \
           _c += 1;                                            \
           usleep(1000);                                       \
        }                                                      \
        BOOST_CHECK((condition));                              \
    }

} /* namespace ovsagent */

#endif /* OVSAGENT_TEST_BASEFIXTURE_H */
