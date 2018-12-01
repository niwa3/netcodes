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
#ifndef MY_QUEUE_ITEM_H
#define MY_QUEUE_ITEM_H

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"
#include "ns3/socket.h"
#include <ns3/address.h>
#include "ns3/nstime.h"

namespace ns3 {

class Packet;
class Socket;

class MyQueueItem : public SimpleRefCount<MyQueueItem>
{
public:
  MyQueueItem (Ptr<Packet> p);

  virtual ~MyQueueItem ();

  Ptr<Packet> GetPacket (void) const;

  virtual uint32_t GetSize (void) const;

  enum Uint8Values
    {
      IP_DSFIELD
    };

  virtual bool GetUint8Value (Uint8Values field, uint8_t &value) const;

  virtual void Print (std::ostream &os) const;

  typedef void (* TracedCallback) (Ptr<const MyQueueItem> item);

private:
  MyQueueItem ();
  MyQueueItem (const MyQueueItem &);
  MyQueueItem &operator = (const MyQueueItem &);

  Ptr<Packet> m_packet;
};

std::ostream& operator<< (std::ostream& os, const MyQueueItem &item);


class MyAppQueueItem: public MyQueueItem {
public:
  MyAppQueueItem (Ptr<Packet> p, Ptr<Socket> socket);

  virtual ~MyAppQueueItem ();

  Ptr<Socket> GetSocket(void) const;

  uint8_t GetQueueIndex (void) const;

  void SetQueueIndex (uint8_t q);

  Time GetTimeStamp (void) const;

  void SetTimeStamp (Time t);

  void Print (std::ostream &os) const;

private:
  MyAppQueueItem ();
  MyAppQueueItem (const MyAppQueueItem &);
  MyAppQueueItem &operator = (const MyAppQueueItem &);

  Ptr<Socket> m_socket;
  uint8_t m_q;          //!< Transmission queue index
  Time m_tstamp;          //!< timestamp when the packet was enqueued
};

} // namespace ns3

#endif /* QUEUE_ITEM_H */
