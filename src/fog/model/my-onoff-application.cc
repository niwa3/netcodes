/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/ipv4.h"
#include "my-onoff-application.h"

#include <sstream>
#include <map>
#include "ns3/json.h"

/**
 * This module is OnOffApplication for my study.
 * Plese look onoff-application.cc if you want to know more detail of this.
 * The difference is that you can set a socket created by youself.
 */

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("MyOnOffApplication");

NS_OBJECT_ENSURE_REGISTERED(MyOnOffApplication);

TypeId
MyOnOffApplication::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::MyOnOffApplication")
    .SetParent<Application>()
    .SetGroupName("Applications")
    .AddConstructor<MyOnOffApplication>()
    .AddAttribute("DataRate", "The data rate in on state.",
                   DataRateValue(DataRate("500kb/s")),
                   MakeDataRateAccessor(&MyOnOffApplication::m_cbrRate),
                   MakeDataRateChecker())
    .AddAttribute("PacketSize", "The size of packets sent in on state",
                   UintegerValue(512),
                   MakeUintegerAccessor(&MyOnOffApplication::m_pktSize),
                   MakeUintegerChecker<uint32_t>(1))
    .AddAttribute("Remote", "The address of the destination",
                   AddressValue(),
                   MakeAddressAccessor(&MyOnOffApplication::m_peer),
                   MakeAddressChecker())
    .AddAttribute("Actuator", "The address of the actuator",
                   AddressValue(),
                   MakeAddressAccessor(&MyOnOffApplication::m_actuator),
                   MakeAddressChecker())
    .AddAttribute("OffTime", "A RandomVariableStream used to pick the duration of the 'Off' state.",
                   StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                   MakePointerAccessor(&MyOnOffApplication::m_offTime),
                   MakePointerChecker <RandomVariableStream>())
    .AddAttribute("MaxBytes", 
                   "The total number of bytes to send. Once these bytes are sent, "
                   "no packet is sent again, even in on state. The value zero means "
                   "that there is no limit.",
                   UintegerValue(0),
                   MakeUintegerAccessor(&MyOnOffApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t>())
    .AddAttribute("Protocol", "The type of protocol to use. This should be "
                   "a subclass of ns3::SocketFactory",
                   TypeIdValue(UdpSocketFactory::GetTypeId()),
                   MakeTypeIdAccessor(&MyOnOffApplication::m_tid),
                   // This should check for SocketFactory as a parent
                   MakeTypeIdChecker())
    .AddAttribute("IsBulkSend", "send a lot of packet",
                   BooleanValue(false),
                   MakeBooleanAccessor(&MyOnOffApplication::m_bulksend),
                   MakeBooleanChecker())
    .AddTraceSource("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor(&MyOnOffApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&MyOnOffApplication::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}


MyOnOffApplication::MyOnOffApplication()
  : m_socket(0),
    m_connected(false),
    m_residualBits(0),
    m_lastStartTime(Seconds(0)),
    m_totBytes(0),
    m_totalRx(0),
    m_clientAddress(Ipv4Address())
{
  NS_LOG_FUNCTION(this);
}

MyOnOffApplication::~MyOnOffApplication()
{
  NS_LOG_FUNCTION(this);
  m_socket = 0;
}

void 
MyOnOffApplication::SetMaxBytes(uint64_t maxBytes)
{
  NS_LOG_FUNCTION(this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
MyOnOffApplication::GetSocket(void) const
{
  NS_LOG_FUNCTION(this);
  return m_socket;
}

int64_t 
MyOnOffApplication::AssignStreams(int64_t stream)
{
  NS_LOG_FUNCTION(this << stream);
  m_offTime->SetStream(stream + 1);
  return 2;
}

void
MyOnOffApplication::DoDispose(void)
{
  NS_LOG_FUNCTION(this);

  m_socket = 0;
  // chain up
  Application::DoDispose();
}

// Application Methods
void
MyOnOffApplication::StartApplication() // Called at time specified by Start
{
  NS_LOG_FUNCTION(this);

  // Create the socket if not already
  if(!m_socket){
    m_socket = Socket::CreateSocket(GetNode(), m_tid);
  }

  if(Inet6SocketAddress::IsMatchingType(m_peer))
  {
    if(m_socket->Bind6() == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
  }
  else if(InetSocketAddress::IsMatchingType(m_peer) ||
          PacketSocketAddress::IsMatchingType(m_peer))
  {
    if(m_socket->Bind() == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
  }
  m_socket->Connect(m_peer);
  m_socket->SetAllowBroadcast(true);

  m_socket->SetConnectCallback(
    MakeCallback(&MyOnOffApplication::ConnectionSucceeded, this),
    MakeCallback(&MyOnOffApplication::ConnectionFailed, this));

  m_cbrRateFailSafe = m_cbrRate;

}

void MyOnOffApplication::StopApplication() // Called at time specified by Stop
{
  NS_LOG_FUNCTION(this);

  CancelEvents();
  if(m_socket != 0)
    {
      m_socket->Close();
    }
  else
    {
      NS_LOG_WARN("MyOnOffApplication found null socket to close in StopApplication");
    }
}

void MyOnOffApplication::CancelEvents()
{
  NS_LOG_FUNCTION(this);

  if(m_sendEvent.IsRunning() && m_cbrRateFailSafe == m_cbrRate )
    { // Cancel the pending send packet event
      // Calculate residual bits since last packet sent
      Time delta(Simulator::Now() - m_lastStartTime);
      int64x64_t bits = delta.To(Time::S) * m_cbrRate.GetBitRate();
      m_residualBits += bits.GetHigh();
    }
  m_cbrRateFailSafe = m_cbrRate;
  Simulator::Cancel(m_sendEvent);
  Simulator::Cancel(m_startStopEvent);
}

// Event handlers
void MyOnOffApplication::StartSending()
{
  NS_LOG_FUNCTION(this);
  m_lastStartTime = Simulator::Now();
  SendPacket();
  if(!m_bulksend){
    ScheduleStartEvent();
  }
}

void MyOnOffApplication::StopSending()
{
  NS_LOG_FUNCTION(this);
  CancelEvents();

  ScheduleStartEvent();
}

// Private helpers
// これを呼ばずに直接sendpacketを一回だけよぶ
void MyOnOffApplication::ScheduleNextTx()
{
  NS_LOG_FUNCTION(this);

  if(m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
      uint32_t bits = m_pktSize * 8 - m_residualBits;
      NS_LOG_LOGIC("bits = " << bits);
      Time nextTime(Seconds(bits /
                              static_cast<double>(m_cbrRate.GetBitRate()))); // Time till next packet
      NS_LOG_LOGIC("nextTime = " << nextTime);
      m_sendEvent = Simulator::Schedule(nextTime,
                                         &MyOnOffApplication::SendPacket, this);
    }
  else
    { // All done, cancel any pending events
      StopApplication();
    }
}

void MyOnOffApplication::ScheduleStartEvent()
{  // Schedules the event to start sending data(switch to the "On" state)
  NS_LOG_FUNCTION(this);

  Time offInterval = MicroSeconds(m_offTime->GetValue());
  NS_LOG_LOGIC("start at " << offInterval);
  m_startStopEvent = Simulator::Schedule(offInterval, &MyOnOffApplication::StartSending, this);
}

void MyOnOffApplication::SendPacket()
{
  NS_LOG_FUNCTION(this);

  NS_ASSERT(m_sendEvent.IsExpired());
  Ptr<Packet> packet = CreatePacket(m_pktSize, m_actuator);
  m_txTrace(packet);
  int sendSize = m_socket->Send(packet);
  NS_LOG_DEBUG("HttpClient (" << m_clientAddress << ") >> Sending request for "
              << "server (" << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << ") size: "<<sendSize << ".");
  m_totBytes += m_pktSize;
  m_lastStartTime = Simulator::Now();
  m_residualBits = 0;
  if(m_bulksend==true){
    ScheduleNextTx();
  }
}

void MyOnOffApplication::SetSocket(Ptr<Socket> socket)
{
  m_socket = socket;
}

void MyOnOffApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  m_clientAddress = socket->GetNode()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
  NS_LOG_DEBUG ("HttpClient (" << m_clientAddress << ") >> Server accepted connection request!");
  m_connected = true;
  socket->SetRecvCallback(MakeCallback (&MyOnOffApplication::HandleReceive, this));
  CancelEvents();
  ScheduleStartEvent();
}

void MyOnOffApplication::ConnectionFailed(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
}

void MyOnOffApplication::HandleReceive (Ptr<Socket> socket)
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
      m_rxTrace (buff[from]->CreateFragment(0,m_pktSize), from);
      buff[from]->RemoveAtStart(m_pktSize);
    }
  }
}

Ptr<Packet> MyOnOffApplication::CreatePacket(uint32_t pktSize, Address peer){
  Ptr<Packet> packet = Create<Packet>((uint8_t*)CreateData(peer).c_str(),pktSize);
  return packet;
}

std::string MyOnOffApplication::CreateData(Address addr){
  std::stringstream nAddr;
  nAddr << m_clientAddress;
  std::stringstream aAddr;
  aAddr << InetSocketAddress::ConvertFrom(addr).GetIpv4();
  int aPort = InetSocketAddress::ConvertFrom(addr).GetPort();
  std::map<std::string,std::string> nodeId {{"Address", nAddr.str()}};
  json11::Json actId = json11::Json::object({
    {"Address", aAddr.str()},
    {"Port", aPort},
  });
  
  json11::Json obj = json11::Json::object({
    {"NodeId", nodeId},
    {"ActuatorId", actId},
  });
  return obj.dump();
}

}
