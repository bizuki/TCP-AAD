/*
 * Copyright (c) 2011 Adrian Sai-wah Tam
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

#ifndef TCP_OPTION_CWND_H
#define TCP_OPTION_CWND_H

#include "tcp-option.h"

#include "ns3/timer.h"

namespace ns3
{

/**
 * \ingroup tcp
 *
 * Defines the TCP option of kind 8 (timestamp option) as in \RFC{1323}
 */

class TcpOptionCwnd : public TcpOption
{
  public:
    TcpOptionCwnd();
    ~TcpOptionCwnd() override;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    void Print(std::ostream& os) const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

    uint8_t GetKind() const override;
    uint32_t GetSerializedSize() const override;

    /**
     * \brief Get the cwnd stored in the Option
     * \return the cwnd
     */
    uint32_t GetCongestionWindow() const;
    /**
     * \brief Set the cwnd stored in the Option
     * \param ts the cwnd
     */
    void SetCongestionWindow(uint32_t cwnd);

  protected:
    uint32_t m_cwnd; //!< congestion window
};

} // namespace ns3

#endif /* TCP_OPTION_CWND */
