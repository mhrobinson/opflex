/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for ofobjectlistener.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "opflex/logging/OFLogHandler.h"
#include "opflex/c/ofloghandler_c.h"

using opflex::logging::OFLogHandler;

static int getLevel(OFLogHandler::Level level) {
    switch (level) {
    case OFLogHandler::INFO:
        return OF_LOG_INFO;
        break;
    case OFLogHandler::WARNING:
        return OF_LOG_WARNING;
        break;
    case OFLogHandler::ERROR:
        return OF_LOG_ERROR;
        break;
    case OFLogHandler::FATAL:
        return OF_LOG_FATAL;
        break;
    case OFLogHandler::DEBUG:
    default:
        return OF_LOG_DEBUG;
        break;
    }
}

static OFLogHandler::Level getLevel(int level) {
    switch (level) {
    case OF_LOG_INFO:
        return OFLogHandler::INFO;
        break;
    case OF_LOG_WARNING:
        return OFLogHandler::WARNING;
        break;
    case OF_LOG_ERROR:
        return OFLogHandler::ERROR;
        break;
    case OF_LOG_FATAL:
        return OFLogHandler::FATAL;
        break;
    case OF_LOG_DEBUG:
    default:
        return OFLogHandler::DEBUG;
        break;
    }
}


class COFLogHandler : public OFLogHandler {
public:
    COFLogHandler(Level level, loghandler_p callback_)
        : OFLogHandler(level), callback(callback_) {}

    virtual ~COFLogHandler() {}

    virtual void handleMessage(const std::string& file,
                               const int line,
                               const std::string& function,
                               const Level level,
                               const std::string& message) {
        callback(file.c_str(), line, function.c_str(),
                 getLevel(level), message.c_str());
    }

    loghandler_p callback;
};

ofstatus ofloghandler_register(int level, loghandler_p callback) {
    static COFLogHandler logHandler(OFLogHandler::NO_LOGGING, NULL);

    if (callback == NULL)
        return OF_EINVALID_ARG;

    logHandler = COFLogHandler(getLevel(level), callback);
    OFLogHandler::registerHandler(logHandler);
    return OF_ESUCCESS;
}
