/*
 * Copyright (c) 2007 Georgia Tech Research Corporation
 * Copyright (c) 2010 Adrian Sai-wah Tam
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */
#ifndef TCP_SOCKET_ADW_H
#define TCP_SOCKET_ADW_H

#include "ipv4-header.h"
#include "ipv6-header.h"
#include "tcp-socket-state.h"
#include "tcp-socket-base.h"

#include "ns3/data-rate.h"
#include "ns3/node.h"
#include "ns3/sequence-number.h"
#include "ns3/timer.h"
#include "ns3/traced-value.h"

#include <queue>
#include <stdint.h>

namespace ns3
{

/* 
    TODO:
    1) Calculate IAT
    2) Get congestion window size from sender (done)
*/
class TcpSocketAdaptiveWindow : public TcpSocketBase
{
    public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    virtual void ReceivedData(Ptr<Packet> packet, const TcpHeader& tcpHeader);
    virtual void DelAckTimeout();

    protected:
    virtual double GetTimeRatio(double iat);
    virtual double UpdateDelayWindow(double theta);
    virtual Time GetDelayTimeout(double iat);

    uint32_t m_lambda{3};
    double m_alpha{0.75};
    double m_maxTimeout{0.5};

    // iat related
    double m_baseIat{INFINITY};
    double m_theta{INFINITY};
    Time m_lastPacketTime{Time::Min()};


    // delay window
    double m_dwnd{3};
};

} // namespace ns3

#endif /* TCP_SOCKET_ADW_H */
