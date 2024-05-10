#define NS_LOG_APPEND_CONTEXT                                                                      \
    if (m_node)                                                                                    \
    {                                                                                              \
        std::clog << " [node " << m_node->GetId() << "] ";                                         \
    }

#include "tcp-socket-adw.h"

#include "ipv4-end-point.h"
#include "ipv4-route.h"
#include "ipv4-routing-protocol.h"
#include "ipv4.h"
#include "ipv6-end-point.h"
#include "ipv6-l3-protocol.h"
#include "ipv6-route.h"
#include "ipv6-routing-protocol.h"
#include "rtt-estimator.h"
#include "tcp-congestion-ops.h"
#include "tcp-header.h"
#include "tcp-l4-protocol.h"
#include "tcp-option-cwnd.h"
#include "tcp-option-sack-permitted.h"
#include "tcp-option-sack.h"
#include "tcp-option-ts.h"
#include "tcp-option-winscale.h"
#include "tcp-rate-ops.h"
#include "tcp-recovery-ops.h"
#include "tcp-rx-buffer.h"
#include "tcp-tx-buffer.h"

#include "ns3/abort.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulation-singleton.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <math.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpSocketAdaptiveWindow");

NS_OBJECT_ENSURE_REGISTERED(TcpSocketAdaptiveWindow);

TypeId
TcpSocketAdaptiveWindow::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpSocketAdaptiveWindow")
            .SetParent<TcpSocketBase>()
            .SetGroupName("Internet")
            .AddConstructor<TcpSocketAdaptiveWindow>()
            .AddAttribute("Lambda",
                          "Lambda parameter from ADW algo ",
                          UintegerValue(3),
                          MakeUintegerAccessor(&TcpSocketAdaptiveWindow::m_lambda),
                          MakeUintegerChecker<uint32_t>(0))
            .AddAttribute("Alpha",
                          "Alpha parameter from ADW algo (used in smoothing function)",
                          DoubleValue(0.75),
                          MakeDoubleAccessor(&TcpSocketAdaptiveWindow::m_alpha),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("Timeout",
                          "Maximum timout of delayed ACK",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&TcpSocketAdaptiveWindow::m_maxTimeout),
                          MakeDoubleChecker<double>());
    return tid;
}

TypeId
TcpSocketAdaptiveWindow::GetInstanceTypeId() const
{
    return TcpSocketAdaptiveWindow::GetTypeId();
}

void
TcpSocketAdaptiveWindow::ReceivedData(Ptr<Packet> p, const TcpHeader& tcpHeader)
{
    NS_LOG_FUNCTION(this << tcpHeader);
    NS_LOG_DEBUG("Data segment, seq=" << tcpHeader.GetSequenceNumber()
                                      << " pkt size=" << p->GetSize());

    double iat = (Simulator::Now() - m_lastPacketTime).GetSeconds();
    m_baseIat = std::min(iat, m_baseIat);

    UpdateDelayWindow(GetTimeRatio(iat));

    // Put into Rx buffer
    SequenceNumber32 expectedSeq = m_tcb->m_rxBuffer->NextRxSequence();
    if (!m_tcb->m_rxBuffer->Add(p, tcpHeader))
    { // Insert failed: No data or RX buffer full
        if (m_tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD ||
            m_tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE)
        {
            SendEmptyPacket(TcpHeader::ACK | TcpHeader::ECE);
            NS_LOG_DEBUG(TcpSocketState::EcnStateName[m_tcb->m_ecnState] << " -> ECN_SENDING_ECE");
            m_tcb->m_ecnState = TcpSocketState::ECN_SENDING_ECE;
        }
        else
        {
            SendEmptyPacket(TcpHeader::ACK);
        }
        return;
    }
    // Notify app to receive if necessary
    if (expectedSeq < m_tcb->m_rxBuffer->NextRxSequence())
    { // NextRxSeq advanced, we have something to send to the app
        if (!m_shutdownRecv)
        {
            NotifyDataRecv();
        }
        // Handle exceptions
        if (m_closeNotified)
        {
            NS_LOG_WARN("Why TCP " << this << " got data after close notification?");
        }
        // If we received FIN before and now completed all "holes" in rx buffer,
        // invoke peer close procedure
        if (m_tcb->m_rxBuffer->Finished() && (tcpHeader.GetFlags() & TcpHeader::FIN) == 0)
        {
            DoPeerClose();
            return;
        }
    }

    // Now send a new ACK packet acknowledging all received and delivered data
    if (m_tcb->m_rxBuffer->Size() > m_tcb->m_rxBuffer->Available() ||
        m_tcb->m_rxBuffer->NextRxSequence() > expectedSeq + p->GetSize())
    { // A gap exists in the buffer, or we filled a gap: Always ACK
        m_congestionControl->CwndEvent(m_tcb, TcpSocketState::CA_EVENT_NON_DELAYED_ACK);
        if (m_tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD ||
            m_tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE)
        {
            SendEmptyPacket(TcpHeader::ACK | TcpHeader::ECE);
            NS_LOG_DEBUG(TcpSocketState::EcnStateName[m_tcb->m_ecnState] << " -> ECN_SENDING_ECE");
            m_tcb->m_ecnState = TcpSocketState::ECN_SENDING_ECE;
        }
        else
        {
            SendEmptyPacket(TcpHeader::ACK);
        }
    }
    else
    { // In-sequence packet: ACK if delayed ack count allows
        if (++m_delAckCount >= m_delAckMaxCount)
        {
            m_delAckEvent.Cancel();
            m_delAckCount = 0;
            m_congestionControl->CwndEvent(m_tcb, TcpSocketState::CA_EVENT_NON_DELAYED_ACK);
            if (m_tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD ||
                m_tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE)
            {
                NS_LOG_DEBUG("Congestion algo " << m_congestionControl->GetName());
                SendEmptyPacket(TcpHeader::ACK | TcpHeader::ECE);
                NS_LOG_DEBUG(TcpSocketState::EcnStateName[m_tcb->m_ecnState]
                             << " -> ECN_SENDING_ECE");
                m_tcb->m_ecnState = TcpSocketState::ECN_SENDING_ECE;
            }
            else
            {
                SendEmptyPacket(TcpHeader::ACK);
            }
        }
        else if (!m_delAckEvent.IsExpired())
        {
            m_congestionControl->CwndEvent(m_tcb, TcpSocketState::CA_EVENT_DELAYED_ACK);
        }
        else if (m_delAckEvent.IsExpired())
        {
            m_congestionControl->CwndEvent(m_tcb, TcpSocketState::CA_EVENT_DELAYED_ACK);
            m_delAckEvent =
                Simulator::Schedule(GetDelayTimeout(iat), &TcpSocketAdaptiveWindow::DelAckTimeout, this);
            NS_LOG_LOGIC(
                this << " scheduled delayed ACK at "
                     << (Simulator::Now() + Simulator::GetDelayLeft(m_delAckEvent)).GetSeconds());
        }
    }
}

double TcpSocketAdaptiveWindow::GetTimeRatio(double iat)
{
    double timeRatio;
    
    if (iat < m_lambda * m_baseIat)
    {
        timeRatio = (m_baseIat - iat) / m_baseIat;
    }
    else
    {
        timeRatio = 1 - m_lambda;
    }
    m_theta = (timeRatio - (m_lambda - 1)) / m_lambda;
    
    return m_theta;
}

double TcpSocketAdaptiveWindow::UpdateDelayWindow(double theta)
{
    double R = std::min(m_tcb->m_rcvCwndValue, static_cast<uint32_t>(AdvertisedWindowSize()));
    
    if (m_cwndDiff >= 0) 
    {
        m_dwnd = std::min(m_dwnd + (1 - theta), R);
    } 
    else if (m_cwndDiff < 0) 
    {
        m_dwnd = m_lambda;
    }

    return m_dwnd;
}

Time TcpSocketAdaptiveWindow::GetDelayTimeout(double iat)
{
    double smoothedIat = m_alpha * m_baseIat + (1 - m_alpha) * iat;
    return Seconds(std::min(m_lambda * smoothedIat, m_maxTimeout));
}

void TcpSocketAdaptiveWindow::DelAckTimeout()
{
    UpdateDelayWindow(m_theta);
    TcpSocketBase::DelAckTimeout();
}

}