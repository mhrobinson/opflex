/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for OpflexServerConnection
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <openssl/err.h>
#include <sys/un.h>

#include "opflex/engine/internal/OpflexServerConnection.h"
#include "opflex/engine/internal/OpflexListener.h"
#include "opflex/engine/internal/OpflexHandler.h"
#include "opflex/logging/internal/logging.hpp"
#include "LockGuard.h"

namespace opflex {
namespace engine {
namespace internal {

using std::string;
using yajr::transport::ZeroCopyOpenSSL;

OpflexServerConnection::OpflexServerConnection(OpflexListener* listener_)
    : OpflexConnection(listener_->handlerFactory),
      listener(listener_), peer(NULL) {

}

OpflexServerConnection::~OpflexServerConnection() {

}

void OpflexServerConnection::setRemotePeer(int rc, struct sockaddr_storage& name) {
    if (rc < 0) {
        LOG(ERROR) << "New connection but could not get "
                   << "remote peer IP address" << uv_strerror(rc);
        return;
    }

    if (name.ss_family == AF_UNIX) {
        remote_peer = ((struct sockaddr_un*)&name)->sun_path;
    } else {
        char addrbuffer[INET6_ADDRSTRLEN];
        inet_ntop(name.ss_family,
                  name.ss_family == AF_INET
                  ? (void *) &(((struct sockaddr_in*)&name)->sin_addr)
                  : (void *) &(((struct sockaddr_in6*)&name)->sin6_addr),
                  addrbuffer, INET6_ADDRSTRLEN);
        remote_peer = addrbuffer;
    }

    LOG(INFO) << "[" << getRemotePeer() << "] "
              << "New server connection";
}

const std::string& OpflexServerConnection::getName() {
    return listener->getName();
}

const std::string& OpflexServerConnection::getDomain() {
    return listener->getDomain();
}

uv_loop_t* OpflexServerConnection::loop_selector(void * data) {
    OpflexServerConnection* conn = (OpflexServerConnection*)data;
    return conn->getListener()->getLoop();
}

void OpflexServerConnection::on_state_change(yajr::Peer * p, void * data,
                                             yajr::StateChange::To stateChange,
                                             int error) {
    OpflexServerConnection* conn = (OpflexServerConnection*)data;
    conn->peer = p;
    switch (stateChange) {
    case yajr::StateChange::CONNECT:
        {
            struct sockaddr_storage name;
            int len = sizeof(name);
            int rc = p->getPeerName((struct sockaddr*)&name, &len);
            conn->setRemotePeer(rc, name);
#ifndef SIMPLE_RPC
            ZeroCopyOpenSSL::Ctx* serverCtx = conn->listener->serverCtx.get();
            if (serverCtx)
                ZeroCopyOpenSSL::attachTransport(p, serverCtx);
#endif
            p->startKeepAlive();

            conn->handler->connected();
        }
        break;
    case yajr::StateChange::DISCONNECT:
        LOG(DEBUG) << "[" << conn->getRemotePeer() << "] "
                   << "Disconnected";
        break;
    case yajr::StateChange::TRANSPORT_FAILURE:
        {
            char buf[120];
            ERR_error_string_n(error, buf, sizeof(buf));
            LOG(ERROR) << "[" << conn->getRemotePeer() << "] "
                       << "SSL Connection error: " << buf;
        }
        break;
    case yajr::StateChange::FAILURE:
        LOG(ERROR) << "[" << conn->getRemotePeer() << "] "
                   << "Connection error: " << uv_strerror(error);
        break;
    case yajr::StateChange::DELETE:
        LOG(INFO) << "[" << conn->getRemotePeer() << "] "
                  << "Connection closed";
        conn->getListener()->connectionClosed(conn);
        break;
    }
}

void OpflexServerConnection::disconnect() {
    handler->disconnected();
    OpflexConnection::disconnect();

    if (peer)
        peer->destroy();
}

void OpflexServerConnection::messagesReady() {
    listener->messagesReady();
}

} /* namespace internal */
} /* namespace engine */
} /* namespace opflex */
