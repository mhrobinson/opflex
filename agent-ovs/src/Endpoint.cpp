/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for FSEndpointSource class.
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iomanip>
#include <arpa/inet.h>

#include <boost/foreach.hpp>

#include "Endpoint.h"

namespace ovsagent {

std::ostream & operator<<(std::ostream &os, const Endpoint& ep) {
    os << "Endpoint["
       << "uuid=" << ep.getUUID()
       << ",ips=[";

    bool first = true;
    BOOST_FOREACH(const std::string& ip, ep.getIPs()) {
        if (first) first = false;
        else os << ",";
        os << ip;
    }
    os << "]";

    const boost::optional<opflex::modb::URI>& u = ep.getEgURI();
    if (u)
        os << ",eg=" << u.get().toString();
    const boost::optional<opflex::modb::MAC>& m = ep.getMAC();
    if (m)
        os << ",mac=" << m.get();
    const boost::optional<std::string>& iface = ep.getInterfaceName();
    if (iface)
        os << ",iface=" << iface.get();

    return os;
}

} /* namespace ovsagent */
