/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */

#ifndef MY_TCP_SERVER_H
#define MY_TCP_SERVER_H

#include <map>
#include <array>
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/data-rate.h"
#include "ns3/address.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/my-queue-item.h"

namespace ns3 {

class Address;
class Socket;
class Packet;
class RandomVariableStream;

class MyTcpServer : public Application
{
public:
  static TypeId GetTypeId (void);
  MyTcpServer ();

  virtual ~MyTcpServer ();

  uint64_t GetTotalRx () const;

  Ptr<Socket> GetListeningSocket (void) const;

  std::list<Ptr<Socket> > GetAcceptedSockets (void) const;

  void SetAddressTable(std::map<Address, Address> addrTable);

protected:
  virtual void DoDispose (void);
private:
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  void HandleRead (Ptr<Socket> socket);
  void HandleAccept (Ptr<Socket> socket, const Address& from);
  void HandlePeerClose (Ptr<Socket> socket);
  void HandlePeerError (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;       //!< Listening socket
  std::list<Ptr<Socket> > m_socketList; //!< the accepted sockets

  Address         m_local;        //!< Local address to bind to
  uint64_t        m_totalRx;      //!< Total bytes received
  TypeId          m_tid;          //!< Protocol TypeId

  //--
  // NIWA
  using MyQueue = DropTailQueue<MyAppQueueItem>;

  uint32_t        m_pktSize;
  std::map<Address, Ptr<Packet>> buff;
  Ptr<RandomVariableStream>  m_calctime;      //!< rng for calc time
  bool m_isBusy; //flag that server is now busy or not
  Ipv4Address m_nodeAddress; //own node address
  MyQueue m_jobQueue;
  std::map<Address, Ptr<Socket>> m_nextServiceSocket;
  std::map<Address, Address> m_addrTable;
  std::map<Address, Ptr<Socket>> m_peerSockets;
  EventId m_sendEvent;

  //void Response(Ptr<Packet> packet, Ptr<Socket> socket);
  void Response(Ptr<Packet> packet);
  void SendNext(Ptr<Packet> packet);
  bool HandleRequest(Ptr<Socket> socket, const Address& from);

  std::string PacketDeserialize(Ptr<Packet> packet);
  Address ParseActuator(std::string data);
  Address ParseSource(std::string data);
  Ptr<Socket> CreateSocket(Address peer);

  void ConnectionSucceeded(Ptr<Socket> socket);
  void ConnectionFailed(Ptr<Socket> socket);

  typedef void (* ServiceTimeTracedCallback) (const Time& calcTime);

  /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
  TracedCallback<Ptr<const Packet>> m_txTrace;
  TracedCallback<const Time &> m_serviceTrace;
};

} // namespace ns3

#endif /* MY_TCP_SERVER_H */
