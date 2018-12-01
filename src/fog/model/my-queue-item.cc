/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Universita' degli Studi di Napoli Federico II
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
 * Author: Stefano Avallone <stefano.avallone@unina.it>
 */

#include "my-queue-item.h"
#include "ns3/packet.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyQueueItem");

MyQueueItem::MyQueueItem (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  m_packet = p;
}

MyQueueItem::~MyQueueItem ()
{
  NS_LOG_FUNCTION (this);
  m_packet = 0;
}

Ptr<Packet>
MyQueueItem::GetPacket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_packet;
}

uint32_t
MyQueueItem::GetSize (void) const
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_packet != 0);
  return m_packet->GetSize ();
}

bool
MyQueueItem::GetUint8Value (MyQueueItem::Uint8Values field, uint8_t& value) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
MyQueueItem::Print (std::ostream& os) const
{
  os << GetPacket();
}

std::ostream & operator << (std::ostream &os, const MyQueueItem &item)
{
  item.Print (os);
  return os;
}


MyAppQueueItem::MyAppQueueItem (Ptr<Packet> p, Ptr<Socket> s)
  : MyQueueItem (p),
    m_socket(s),
    m_q (0)
{
  NS_LOG_FUNCTION (this << p << s);
}

MyAppQueueItem::~MyAppQueueItem()
{
  NS_LOG_FUNCTION (this);
}

Ptr<Socket>
MyAppQueueItem::GetSocket(void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

uint8_t
MyAppQueueItem::GetQueueIndex (void) const
{
  NS_LOG_FUNCTION (this);
  return m_q;
}

void
MyAppQueueItem::SetQueueIndex (uint8_t q)
{
  NS_LOG_FUNCTION (this << (uint16_t) q);
  m_q = q;
}

Time
MyAppQueueItem::GetTimeStamp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_tstamp;
}

void
MyAppQueueItem::SetTimeStamp (Time t)
{
  NS_LOG_FUNCTION (this << t);
  m_tstamp = t;
}

void
MyAppQueueItem::Print (std::ostream& os) const
{
  os << GetPacket () << " "
     << "Dst socket " << m_socket<< " "
     << "q " << (uint8_t) m_q
  ;
}

} // namespace ns3
