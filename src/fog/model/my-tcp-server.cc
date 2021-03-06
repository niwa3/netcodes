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
#include "my-tcp-server.h"

#include "sstream"
#include "ns3/json.h"

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
  m_jobQueue.SetMaxPackets(10000000);
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
  m_nodeAddress = GetNode()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();

  for(auto i: m_addrTable){
    NS_LOG_DEBUG("MyTcpServer(" << m_nodeAddress << ") >> start to create socket (end node: " << i.first <<" )");
    if(!static_cast<Address>(i.second).IsInvalid()){
      auto socketItr = m_nextServiceSocket.find(static_cast<Address>(i.second));
      if(socketItr==m_nextServiceSocket.end()){
        NS_LOG_DEBUG("MyTcpServer(" << m_nodeAddress << ") >> create socket to " << i.second);
        m_nextServiceSocket[static_cast<Address>(i.second)] = CreateSocket(static_cast<Address>(i.second));
      }
      else{
        NS_LOG_DEBUG("MyTcpServer(" << m_nodeAddress << ") >> already has a socket to " << i.second);
      }
    }
  }
}

void MyTcpServer::ConnectionSucceeded(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  NS_LOG_DEBUG ("MyTcpServer(" << m_nodeAddress<< ") >> Next Server accepted connection request!");
  //socket->SetRecvCallback(MakeCallback (&MyTcpServer::HandleReceive, this));
}

void MyTcpServer::ConnectionFailed(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
}

void MyTcpServer::StopApplication ()     // Called at time specified by Stop
{
  Simulator::Cancel(m_sendEvent);
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
    NS_LOG_DEBUG("MyTcpServe(" << m_nodeAddress << ") >> received packet " << packet->GetSize() << " from " << from << " " << __FILE__);
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
      NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> receive a packet from "<<InetSocketAddress::ConvertFrom(from).GetIpv4());
      if(m_jobQueue.IsEmpty() && !m_isBusy){
        m_isBusy = true;
        Time calcInterval = MicroSeconds(m_calctime->GetValue());
        m_serviceTrace(calcInterval);
        NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> start...");
        if(m_nextServiceSocket.empty()){
          m_sendEvent = Simulator::Schedule (calcInterval, &MyTcpServer::Response, this, receivedPacket);
        }
        else{
          m_sendEvent = Simulator::Schedule (calcInterval, &MyTcpServer::SendNext, this, receivedPacket);
        }
      }
      else{
        Ptr<MyAppQueueItem> newJob = Create<MyAppQueueItem>(receivedPacket, socket);
        NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> enqueue... (size: "<<m_jobQueue.GetCurrentSize()<<"/"<<m_jobQueue.GetMaxPackets()<<")");
        m_jobQueue.Enqueue(newJob);
      }
    }
  }
}

void MyTcpServer::Response(Ptr<Packet> packet){
  NS_LOG_FUNCTION(this);
  //TODO
  //you can add the logic to create response packet
  Ptr<Packet> rePacket = packet;

  Address peer = ParseActuator(PacketDeserialize(packet));

  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> get actuator address "<<InetSocketAddress::ConvertFrom(peer).GetIpv4()<<" port "<<InetSocketAddress::ConvertFrom(peer).GetPort());

  Ptr<Socket> distSocket;
  auto socketItr = m_peerSockets.find(peer);
  if(socketItr==m_peerSockets.end()){
    distSocket = CreateSocket(peer);
    m_peerSockets[peer] = distSocket;
  }
  else{
    distSocket = socketItr->second;
  }
  int sendSize = distSocket->Send(rePacket);
  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> send a packet to "<<InetSocketAddress::ConvertFrom(peer).GetIpv4()<<" size: "<<sendSize);
  m_txTrace(rePacket);
  m_isBusy = false;
  if(m_jobQueue.IsEmpty()){
    return;
  }
  else{
    m_isBusy = true;
    Ptr<MyAppQueueItem> item = m_jobQueue.Dequeue();
    NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> dequeue... (size: "<<m_jobQueue.GetCurrentSize()<<"/"<<m_jobQueue.GetMaxPackets()<<")");
    Time calcInterval = MicroSeconds(m_calctime->GetValue());
    m_serviceTrace(calcInterval);
    NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> start...");
    m_sendEvent = Simulator::Schedule (calcInterval, &MyTcpServer::Response, this, item->GetPacket());
  }
}

void MyTcpServer::SendNext(Ptr<Packet> packet){
  NS_LOG_FUNCTION(this);
  //TODO
  //you can add the logic to create response packet
  //Ptr<Packet> rePacket = Create<Packet>(m_pktSize);
  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> parse packet");
  Address next = ParseSource(PacketDeserialize(packet));
  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> start to send a packet from "<< InetSocketAddress::ConvertFrom(next).GetIpv4());
  Ptr<Packet> rePacket = packet;
  int sendSize = m_nextServiceSocket[m_addrTable[next]]->Send(rePacket);

  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> send a packet to "<<InetSocketAddress::ConvertFrom(m_addrTable[next]).GetIpv4()<<" size: "<<sendSize);
  m_txTrace(rePacket);
  m_isBusy = false;
  if(m_jobQueue.IsEmpty()){
    return;
  }
  else{
    m_isBusy = true;
    Ptr<MyAppQueueItem> item = m_jobQueue.Dequeue();
    NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> dequeue... (size: "<<m_jobQueue.GetCurrentSize()<<"/"<<m_jobQueue.GetMaxPackets()<<")");
    Time calcInterval = MicroSeconds(m_calctime->GetValue());
    m_serviceTrace(calcInterval);
    NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> start...");
    m_sendEvent = Simulator::Schedule (calcInterval, &MyTcpServer::SendNext, this, item->GetPacket());
  }
}

void MyTcpServer::HandlePeerClose (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address address;
  socket->GetPeerName(address);
  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> Connection with Client (" <<  InetSocketAddress::ConvertFrom (address).GetIpv4 () << ") close!");
}

void MyTcpServer::HandlePeerError (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

void MyTcpServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_FUNCTION (this << s << from);
  NS_LOG_DEBUG("MyTcpServer("<<m_nodeAddress<<") >> Connection with Client (" <<  InetSocketAddress::ConvertFrom (from).GetIpv4 () << ") successfully established!");
  s->SetRecvCallback (MakeCallback (&MyTcpServer::HandleRead, this));
  m_socketList.push_back (s);
}

bool MyTcpServer::HandleRequest (Ptr<Socket> s, const Address& address)
{
  NS_LOG_FUNCTION (this << s << address);
  NS_LOG_DEBUG ("MyTcpServer("<<m_nodeAddress<<") >> Request for connection from " << InetSocketAddress::ConvertFrom (address).GetIpv4 () << " received.");
  return true;
}

void MyTcpServer::SetAddressTable(std::map<Address, Address> addrTable){
  m_addrTable = addrTable;
}

std::string MyTcpServer::PacketDeserialize(Ptr<Packet> p){
  uint8_t buf[m_pktSize];
  p->CopyData(buf, m_pktSize);
  std::stringstream text;
  text << buf;
  return text.str();
}

Address MyTcpServer::ParseActuator(std::string data){
  std::string err;
  auto json = json11::Json::parse(data, err);
  Ipv4Address aAddr(json["ActuatorId"]["Address"].string_value().c_str());
  return InetSocketAddress(aAddr, json["ActuatorId"]["Port"].int_value());
}

Address MyTcpServer::ParseSource(std::string data){
  std::string err;
  auto json = json11::Json::parse(data, err);
  Ipv4Address aAddr(json["NodeId"]["Address"].string_value().c_str());
  return InetSocketAddress(aAddr);
}

Ptr<Socket> MyTcpServer::CreateSocket(Address peer)
{
  NS_LOG_FUNCTION (this);

  Ptr<Socket> s = Socket::CreateSocket (GetNode (), m_tid);
  if(!peer.IsInvalid()){
    if(InetSocketAddress::IsMatchingType(peer) ||
             PacketSocketAddress::IsMatchingType(peer))
      {
        if(s->Bind() == -1)
          {
            NS_FATAL_ERROR("Failed to bind socket");
          }
      }
    NS_LOG_DEBUG("try to connect");
    s->Connect(peer);
    //s->SetAllowBroadcast(true);

    s->SetConnectCallback(
      MakeCallback(&MyTcpServer::ConnectionSucceeded, this),
      MakeCallback(&MyTcpServer::ConnectionFailed, this));
  }
  return s;
}

} // Namespace ns3
