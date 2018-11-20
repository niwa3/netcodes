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

#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/internet-module.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "my-tcp-server.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyTcpServer");

NS_OBJECT_ENSURE_REGISTERED (MyTcpServer);

TypeId 
MyTcpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyTcpServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MyTcpServer> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&MyTcpServer::m_local),
                   MakeAddressChecker ())
    .AddAttribute("PacketSize", "The size of packets sent in on state",
                   UintegerValue(512),
                   MakeUintegerAccessor(&MyTcpServer::m_pktSize),
                   MakeUintegerChecker<uint32_t>(1))
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MyTcpServer::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute("CalcTime", "A RandomVariableStream used to pick the duration of the calculation.[ns]",
                   StringValue("ns3::ExponentialRandomVariable[Mean=1.0]"),
                   MakePointerAccessor(&MyTcpServer::m_calctime),
                   MakePointerChecker <RandomVariableStream>())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&MyTcpServer::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddTraceSource("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor(&MyTcpServer::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource("ServiceTime", "Service Time",
                     MakeTraceSourceAccessor(&MyTcpServer::m_serviceTrace),
                     "ns3::MyTcpServer::ServiceTimeTracedCallback")
  ;
  return tid;
}

MyTcpServer::MyTcpServer ()
  : m_isBusy(false)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
  m_jobQueue.SetMaxPackets(1000);
}

MyTcpServer::~MyTcpServer()
{
  NS_LOG_FUNCTION (this);
}

uint64_t MyTcpServer::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
MyTcpServer::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket>>
MyTcpServer::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void MyTcpServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void MyTcpServer::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket (GetNode (), m_tid);
    if (m_socket->Bind (m_local) == -1)
      {
        NS_FATAL_ERROR ("Failed to bind socket");
      }
    if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
        m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
      {
        NS_FATAL_ERROR ("Using HttpServer with an incompatible socket type. "
                        "HttpServer requires SOCK_STREAM or SOCK_SEQPACKET. "
                        "In other words, use TCP instead of UDP.");
      }
    m_socket->Listen ();
    //m_socket->ShutdownSend ();

    m_socket->SetAcceptCallback (
      MakeCallback (&MyTcpServer::HandleRequest, this),
      MakeCallback (&MyTcpServer::HandleAccept, this));
    m_socket->SetCloseCallbacks (
      MakeCallback (&MyTcpServer::HandlePeerClose, this),
      MakeCallback (&MyTcpServer::HandlePeerError, this));
  }
}

void MyTcpServer::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  while(!m_socketList.empty ()) //these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front ();
      m_socketList.pop_front ();
      acceptedSocket->Close ();
    }
  if (m_socket) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void MyTcpServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_DEBUG("received packet " << packet->GetSize() << " from " << from << " " << __FILE__);
      if (packet->GetSize () == 0)
        { //EOF
          break;
        }
      m_totalRx += packet->GetSize ();
      if(!buff.count(from)){
        buff[from] = packet;
      }
      else{
        buff[from]->AddAtEnd(packet);
      }
      NS_LOG_DEBUG("from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " size " << buff[from]->GetSize());
      if(buff[from]->GetSize()>=m_pktSize){
        Ptr<Packet> receivedPacket = buff[from]->CreateFragment(0,m_pktSize);
        buff[from]->RemoveAtStart(m_pktSize);
        m_rxTrace(receivedPacket, from);
        NS_LOG_DEBUG("MyTcpServer >> receive a packet from "<<InetSocketAddress::ConvertFrom(from).GetIpv4());
        if(m_jobQueue.IsEmpty() && !m_isBusy){
          m_isBusy = true;
          Time calcInterval = MicroSeconds(m_calctime->GetValue());
          m_serviceTrace(calcInterval);
          NS_LOG_DEBUG("MyTcpServer >> start...");
          Simulator::Schedule (calcInterval, &MyTcpServer::Response, this, receivedPacket, socket);
        }
        else{
          Ptr<MyAppQueueItem> newJob = Create<MyAppQueueItem>(receivedPacket, socket);
          NS_LOG_DEBUG("MyTcpServer >> enqueue... (size: "<<m_jobQueue.GetCurrentSize()<<"/"<<m_jobQueue.GetMaxPackets()<<")");
          m_jobQueue.Enqueue(newJob);
        }
      }
    }
}

void MyTcpServer::Response(Ptr<Packet> packet, Ptr<Socket> socket){
  NS_LOG_FUNCTION(this << socket);
  //TODO
  //you can add the logic to create response packet
  Ptr<Packet> rePacket = Create<Packet>(m_pktSize);
  Address peerAddress;
  socket->GetPeerName(peerAddress);
  int sendSize = socket->Send(rePacket);
  NS_LOG_DEBUG("MyTcpServer >> send a packet to "<<InetSocketAddress::ConvertFrom(peerAddress).GetIpv4()<<" size: "<<sendSize);
  m_txTrace(rePacket);
  m_isBusy = false;
  if(m_jobQueue.IsEmpty()){
    return;
  }
  else{
    m_isBusy = true;
    Ptr<MyAppQueueItem> item = m_jobQueue.Dequeue();
    NS_LOG_DEBUG("MyTcpServer >> dequeue... (size: "<<m_jobQueue.GetCurrentSize()<<"/"<<m_jobQueue.GetMaxPackets()<<")");
    Time calcInterval = MicroSeconds(m_calctime->GetValue());
    m_serviceTrace(calcInterval);
    NS_LOG_DEBUG("MyTcpServer >> start...");
    Simulator::Schedule (calcInterval, &MyTcpServer::Response, this, item->GetPacket(), item->GetSocket());
  }
}

void MyTcpServer::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address address;
  socket->GetPeerName(address);
  NS_LOG_DEBUG("MyTcpServer >> Connection with Client (" <<  InetSocketAddress::ConvertFrom (address).GetIpv4 () << ") close!");
}

void MyTcpServer::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void MyTcpServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  NS_LOG_DEBUG("MyTcpServer >> Connection with Client (" <<  InetSocketAddress::ConvertFrom (from).GetIpv4 () << ") successfully established!");
  s->SetRecvCallback (MakeCallback (&MyTcpServer::HandleRead, this));
  m_socketList.push_back (s);
}

bool MyTcpServer::HandleRequest (Ptr<Socket> s, const Address& address)
{
  NS_LOG_FUNCTION (this << s << address);
  NS_LOG_DEBUG ("MyTcpServer >> Request for connection from " << InetSocketAddress::ConvertFrom (address).GetIpv4 () << " received.");
  return true;
}

} // Namespace ns3
