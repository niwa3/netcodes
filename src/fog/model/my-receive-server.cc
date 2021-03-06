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
#include "ns3/packet-socket-address.h"
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

#include "my-receive-server.h"

#include "sstream"
#include "ns3/json.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyReceiveServer");

NS_OBJECT_ENSURE_REGISTERED (MyReceiveServer);

TypeId 
MyReceiveServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyReceiveServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MyReceiveServer> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&MyReceiveServer::m_local),
                   MakeAddressChecker ())
    .AddAttribute("PacketSize", "The size of packets sent in on state",
                   UintegerValue(512),
                   MakeUintegerAccessor(&MyReceiveServer::m_pktSize),
                   MakeUintegerChecker<uint32_t>(1))
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&MyReceiveServer::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&MyReceiveServer::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddTraceSource("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor(&MyReceiveServer::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

MyReceiveServer::MyReceiveServer ()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_totalRx = 0;
}

MyReceiveServer::~MyReceiveServer()
{
  NS_LOG_FUNCTION (this);
}

uint64_t MyReceiveServer::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

Ptr<Socket>
MyReceiveServer::GetListeningSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

std::list<Ptr<Socket>>
MyReceiveServer::GetAcceptedSockets (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socketList;
}

void MyReceiveServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  m_socketList.clear ();

  // chain up
  Application::DoDispose ();
}


// Application Methods
void MyReceiveServer::StartApplication ()    // Called at time specified by Start
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
    m_socket->ShutdownSend ();

    m_socket->SetAcceptCallback (
      MakeCallback (&MyReceiveServer::HandleRequest, this),
      MakeCallback (&MyReceiveServer::HandleAccept, this));
    m_socket->SetCloseCallbacks (
      MakeCallback (&MyReceiveServer::HandlePeerClose, this),
      MakeCallback (&MyReceiveServer::HandlePeerError, this));
  }
  m_nodeAddress = GetNode()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
}

void MyReceiveServer::StopApplication ()     // Called at time specified by Stop
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

void MyReceiveServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_DEBUG("MyReceiveServer(" << m_nodeAddress << ") >> received packet " << packet->GetSize() << " from " << from << " " << __FILE__);
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
        NS_LOG_DEBUG("MyReceiveServer("<<m_nodeAddress<<") >> receive a packet from "<<InetSocketAddress::ConvertFrom(from).GetIpv4());
      }
    }
}

void MyReceiveServer::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address address;
  socket->GetPeerName(address);
  NS_LOG_DEBUG("MyReceiveServer("<<m_nodeAddress<<") >> Connection with Client (" <<  InetSocketAddress::ConvertFrom (address).GetIpv4 () << ") close!");
}

void MyReceiveServer::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void MyReceiveServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  NS_LOG_DEBUG("MyReceiveServer("<<m_nodeAddress<<") >> Connection with Client (" <<  InetSocketAddress::ConvertFrom (from).GetIpv4 () << ") successfully established!");
  s->SetRecvCallback (MakeCallback (&MyReceiveServer::HandleRead, this));
  m_socketList.push_back (s);
}

bool MyReceiveServer::HandleRequest (Ptr<Socket> s, const Address& address)
{
  NS_LOG_FUNCTION (this << s << address);
  NS_LOG_DEBUG ("MyReceiveServer("<<m_nodeAddress<<") >> Request for connection from " << InetSocketAddress::ConvertFrom (address).GetIpv4 () << " received.");
  return true;
}

std::string MyReceiveServer::PacketDeserialize(Ptr<Packet> p){
  uint8_t buf[m_pktSize];
  p->CopyData(buf, m_pktSize);
  std::stringstream text;
  text << buf;
  return text.str();
}

Address MyReceiveServer::ParseData(std::string data){
  std::string err;
  auto json = json11::Json::parse(data, err);
  Ipv4Address aAddr(json["ActuatorId"]["Address"].string_value().c_str());
  return InetSocketAddress(aAddr, json["ActuatorId"]["Port"].int_value());
}

} // Namespace ns3
