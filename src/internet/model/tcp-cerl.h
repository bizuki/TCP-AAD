#ifndef TCPCERL_H
#define TCPCERL_H

#include "tcp-congestion-ops.h"

namespace ns3
{

class TcpCerl : public TcpCongestionOps
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TcpCerl();

    /**
     * \brief Copy constructor.
     * \param sock object to copy.
     */
    TcpCerl(const TcpCerl& sock);

    ~TcpCerl() override;

    std::string GetName() const override;

    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
    void CongestionStateSet(Ptr<TcpSocketState> tcb,
                            const TcpSocketState::TcpCongState_t newState) override;
    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
    void EnterRecovery(Ptr<TcpSocketState> tcb) override;
    void ExitRecovery(Ptr<TcpSocketState> tcb) override;
    Ptr<TcpCongestionOps> Fork() override;
    virtual double GetA();

  protected:
    virtual uint32_t SlowStart(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
    virtual void CongestionAvoidance(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
    virtual bool isCongestionLoss();

  protected:
    int32_t m_oldCwnd{-1};
    Time m_lMax{Time::Min()};
    Time m_l;
    Time m_rttMin{Time::Max()};
    double m_A{0.55};
    bool m_enteredRecovery{false};
    bool m_decreaseCwnd{false};
    SequenceNumber32 m_maxSentSeq{0};
};

class TcpCerlPlus : public TcpCerl
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TcpCerlPlus();

    /**
     * \brief Copy constructor.
     * \param sock object to copy.
     */
    TcpCerlPlus(const TcpCerl& sock);

    ~TcpCerlPlus() override;

    std::string GetName() const override;
    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
  protected:
    virtual double GetA() override;
    Time m_rttSum{0};
    uint32_t m_sampleSize{0};
};

class TcpCerlX : public TcpCerlPlus
{
  public:
    struct PktInfo
    {
      Time rtt;
      Time ts;
    };

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    TcpCerlX();

    /**
     * \brief Copy constructor.
     * \param sock object to copy.
     */
    TcpCerlX(const TcpCerlPlus& sock);

    ~TcpCerlX() override;

    std::string GetName() const override;
    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;

  private:
    void AddPkt(PktInfo&& info);

    std::vector<PktInfo> m_samples;
    std::set<Time> m_sampleRtts;
};

} // namespace ns3

#endif // TCPCERL_H
