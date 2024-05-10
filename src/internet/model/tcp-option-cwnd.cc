#include "tcp-option-cwnd.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpOptionCwnd");

NS_OBJECT_ENSURE_REGISTERED(TcpOptionCwnd);

TcpOptionCwnd::TcpOptionCwnd()
    : TcpOption(),
      m_cwnd(0)
{
}

TcpOptionCwnd::~TcpOptionCwnd()
{
}

TypeId
TcpOptionCwnd::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpOptionCwnd")
                            .SetParent<TcpOption>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpOptionCwnd>();
    return tid;
}

TypeId
TcpOptionCwnd::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
TcpOptionCwnd::Print(std::ostream& os) const
{
    os << m_cwnd;
}

uint32_t
TcpOptionCwnd::GetSerializedSize() const
{
    return 6;
}

void
TcpOptionCwnd::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU8(GetKind());        // Kind
    i.WriteU8(6);                // Length
    i.WriteHtonU32(m_cwnd);      // cwnd
}

uint32_t
TcpOptionCwnd::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;

    uint8_t readKind = i.ReadU8();
    if (readKind != GetKind())
    {
        NS_LOG_WARN("Malformed CongestionWindow option");
        return 0;
    }

    uint8_t size = i.ReadU8();
    if (size != 6)
    {
        NS_LOG_WARN("Malformed CongestionWindow option");
        return 0;
    }
    m_cwnd = i.ReadNtohU32();
    return GetSerializedSize();
}

uint8_t
TcpOptionCwnd::GetKind() const
{
    return TcpOption::CWND;
}

uint32_t
TcpOptionCwnd::GetCongestionWindow() const
{
    return m_cwnd;
}

void
TcpOptionCwnd::SetCongestionWindow(uint32_t cwnd)
{
    m_cwnd = cwnd;
}

} // namespace ns3
