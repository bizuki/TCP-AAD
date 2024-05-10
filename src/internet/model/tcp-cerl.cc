#include "tcp-cerl.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

NS_LOG_COMPONENT_DEFINE("TcpCerl");


namespace ns3
{
NS_OBJECT_ENSURE_REGISTERED(TcpCerl);

TypeId
TcpCerl::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpCerl")
                            .SetParent<TcpCongestionOps>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpCerl>();
    return tid;
}

TcpCerl::TcpCerl()
    : TcpCongestionOps()
{
    NS_LOG_FUNCTION(this);
}

TcpCerl::TcpCerl(const TcpCerl& sock)
    : TcpCongestionOps(sock)
{
    NS_LOG_FUNCTION(this);
}

TcpCerl::~TcpCerl()
{
}

uint32_t
TcpCerl::SlowStart(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked >= 1)
    {
        tcb->m_cWnd += tcb->m_segmentSize;
        NS_LOG_INFO("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh "
                                                     << tcb->m_ssThresh);
        return segmentsAcked - 1;
    }

    return 0;
}

void
TcpCerl::CongestionAvoidance(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (segmentsAcked > 0)
    {
        double adder =
            static_cast<double>(tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get();
        adder = std::max(1.0, adder);
        tcb->m_cWnd += static_cast<uint32_t>(adder);
        NS_LOG_INFO("In CongAvoid, updated to cwnd " << tcb->m_cWnd << " ssthresh "
                                                     << tcb->m_ssThresh);
    }
}

void
TcpCerl::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    if (tcb->m_cWnd < tcb->m_ssThresh)
    {
        segmentsAcked = SlowStart(tcb, segmentsAcked);
    }

    if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
        CongestionAvoidance(tcb, segmentsAcked);
    }
}

std::string
TcpCerl::GetName() const
{
    return "TcpCerl";
}

uint32_t
TcpCerl::GetSsThresh(Ptr<const TcpSocketState> state, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << state << bytesInFlight);

    NS_ASSERT(state->m_lastAckedSeq.GetValue() > 0);
    if (!m_enteredRecovery) 
    {
        return std::max(2 * state->m_segmentSize, bytesInFlight / 2);
    } 
    else 
    {
        bool isCongestion = isCongestionLoss();
        NS_LOG_DEBUG("isCongestionLoss() = " << isCongestion);
        if (state->m_lastAckedSeq - 1 > m_maxSentSeq && isCongestion)
        {
            NS_LOG_DEBUG("Congestion loss in new window (" << state->m_lastAckedSeq << " - 1 > " << m_maxSentSeq << "). Update m_maxSentSeq = " << state->m_highTxMark);
            m_maxSentSeq = state->m_highTxMark;
            m_decreaseCwnd = true;
            return std::max(2 * state->m_segmentSize, bytesInFlight / 2);
        }
        else 
        {
            m_oldCwnd = state->m_cWnd.Get();
            return state->m_ssThresh;
        }
    }
}

void
TcpCerl::CongestionStateSet(Ptr<TcpSocketState> tcb,
                                     const TcpSocketState::TcpCongState_t newState)
{
    NS_LOG_FUNCTION(this << tcb << newState);
    if (newState == TcpSocketState::CA_RECOVERY) {
        m_enteredRecovery = true;
    }
}

void 
TcpCerl::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) 
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);
    NS_LOG_DEBUG("HighAcked = " << tcb->m_lastAckedSeq);
    m_rttMin = std::min(rtt, m_rttMin);

    m_l = rtt - m_rttMin;
    m_lMax = std::max(m_lMax, m_l);
}

void
TcpCerl::EnterRecovery(Ptr<TcpSocketState> state)
{
    NS_LOG_FUNCTION(this << state);
    if (!m_enteredRecovery) 
    {
        state->m_cWnd = state->m_ssThresh.Get();
    } 
    else 
    {
        NS_LOG_DEBUG("Entered recovery due to 3 dupacks");
        m_enteredRecovery = false;
        if (m_decreaseCwnd) 
        {
            NS_LOG_DEBUG("Decreased cwnd");
            state->m_cWnd = state->m_ssThresh.Get();
            m_decreaseCwnd = false;
        }

    }
    NS_LOG_DEBUG("Inflating window. Set congestion window to = "
                            << state->m_cWnd);
}

void
TcpCerl::ExitRecovery(Ptr<TcpSocketState> state)
{
    NS_LOG_FUNCTION(this << state);
    if (m_oldCwnd >= 0) 
    {
        state->m_cWnd = m_oldCwnd;
        m_oldCwnd = -1;
        NS_LOG_DEBUG("OldCwnd is present, set cwnd to it");
    }
    else
    {
        state->m_cWnd = state->m_ssThresh.Get();
    }
    NS_LOG_DEBUG("Deflating window. Set congestion window to = "
                            << state->m_cWnd);
}

Ptr<TcpCongestionOps>
TcpCerl::Fork()
{
    return CopyObject<TcpCerl>(this);
}

bool 
TcpCerl::isCongestionLoss()
{
    return m_lMax * GetA() <= m_l;
}

double 
TcpCerl::GetA()
{
    return m_A;
}

NS_OBJECT_ENSURE_REGISTERED(TcpCerlPlus);

TypeId
TcpCerlPlus::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpCerlPlus")
                            .SetParent<TcpCongestionOps>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpCerlPlus>();
    return tid;
}

TcpCerlPlus::TcpCerlPlus()
    : TcpCerl()
{
    NS_LOG_FUNCTION(this);
}

TcpCerlPlus::TcpCerlPlus(const TcpCerl& sock)
    : TcpCerl(sock)
{
    NS_LOG_FUNCTION(this);
}

TcpCerlPlus::~TcpCerlPlus()
{
}

std::string
TcpCerlPlus::GetName() const
{
    return "TcpCerlPlus";
}

void 
TcpCerlPlus::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) 
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);
    TcpCerl::PktsAcked(tcb, segmentsAcked, rtt);
    m_rttSum += rtt;
    m_sampleSize++;
}

double 
TcpCerlPlus::GetA()
{
    NS_LOG_FUNCTION(this);
    return ((m_rttMin * m_sampleSize) / m_rttSum).GetDouble();
}

NS_OBJECT_ENSURE_REGISTERED(TcpCerlX);

TypeId
TcpCerlX::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpCerlX")
                            .SetParent<TcpCongestionOps>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpCerlX>()
                            .AddAttribute("WindowSize",
                            "Value of window size in seconds",
                            UintegerValue(60),
                            MakeUintegerAccessor(&TcpCerlX::m_windowSize),
                            MakeUintegerChecker<uint32_t>());
    return tid;
}

TcpCerlX::TcpCerlX()
    : TcpCerlPlus()
{
    NS_LOG_FUNCTION(this);
}

TcpCerlX::TcpCerlX(const TcpCerlPlus& sock)
    : TcpCerlPlus(sock)
{
    NS_LOG_FUNCTION(this);
}

TcpCerlX::~TcpCerlX()
{
}

std::string
TcpCerlX::GetName() const
{
    return "TcpCerlX";
}

void 
TcpCerlX::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) 
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);
    TcpCerlPlus::PktsAcked(tcb, segmentsAcked, rtt);
    AddPkt(TcpCerlX::PktInfo{rtt, Simulator::Now()});
}

void 
TcpCerlX::AddPkt(TcpCerlX::PktInfo&& info) 
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("Adding packet " << m_sampleRtts.size() << " " << info.rtt);
    m_samples.push_back(info);
    m_sampleRtts.insert(info.rtt);
    NS_LOG_DEBUG("Removing packet" << m_windowSize);
    if (m_samples.size() > 2 && (Simulator::Now() - m_samples.front().ts).GetSeconds() > m_windowSize) 
    {
        NS_LOG_DEBUG("Removing packet");
        auto& lastSample = m_samples.front();
        m_sampleRtts.erase(m_sampleRtts.find(lastSample.rtt));
        m_samples.pop_front();
    
        m_rttMin = *m_sampleRtts.begin();
        m_lMax = *m_sampleRtts.rbegin() - m_rttMin;
    
        m_rttSum -= lastSample.rtt;
        m_sampleSize--;
    }
}

} // namespace ns3
