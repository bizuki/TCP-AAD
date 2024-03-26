/*
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
 */

#include "app.h"

#include "ns3/applications-module.h"

using namespace ns3;

TcpSender::TcpSender()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

TcpSender::~TcpSender()
{
    m_socket = nullptr;
}

/* static */
TypeId
TcpSender::GetTypeId()
{
    static TypeId tid = TypeId("TcpSender")
                            .SetParent<Application>()
                            .SetGroupName("Experiments")
                            .AddConstructor<TcpSender>();
    return tid;
}

void
TcpSender::Setup(Ptr<Socket> socket,
                   uint32_t port,
                   Address address,
                   uint32_t packetSize,
                   uint32_t nPackets,
                   DataRate dataRate)
{
    m_socket = socket;
    m_port = port;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_dataRate = dataRate;
}

void
TcpSender::StartApplication()
{
    m_running = true;
    m_packetsSent = 0;
    m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_port));
    m_socket->Connect(m_peer);
    SendPacket();
}

void
TcpSender::StopApplication()
{
    m_running = false;

    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void
TcpSender::SendPacket()
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    if (++m_packetsSent < m_nPackets)
    {
        ScheduleTx();
    }
}

void
TcpSender::ScheduleTx()
{
    if (m_running)
    {
        Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        // std::cout << (Simulator::Now() + tNext).GetSeconds() << std::endl;
        m_sendEvent = Simulator::Schedule(tNext, &TcpSender::SendPacket, this);
    }
}
