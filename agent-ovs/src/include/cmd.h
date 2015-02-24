/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Utility functions for command-line programs
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

namespace ovsagent {

/**
 * Daemonize a process
 */
void daemonize();

/**
 * Initialize logging
 *
 * @param level the log level to log at
 * @param log_file the file to log to, or empty for console
 */
void initLogging(const std::string& level, const std::string& log_file);

/**
 * Change the logging level of the agent.
 *
 * @param level the log level to log at. If this is not a valid string,
 * logging level is set to the default level.
 */
void setLoggingLevel(const std::string& level);

} /* namespace ovsagent */