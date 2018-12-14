/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MY_ONOFF_APPLICATION_H
#define MY_ONOFF_APPLICATION_H

#include <map>
#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"


namespace ns3 {

class Address;
class RandomVariableStream;
class Socket;


class MyOnOffApplication : public Application 
{
public:
  static TypeId GetTypeId(void);

  MyOnOffApplication();

  virtual ~MyOnOffApplication();

  void SetMaxBytes(uint64_t maxBytes);

  Ptr<Socket> GetSocket(void) const;

  int64_t AssignStreams(int64_t stream);

  void SetSocket(Ptr<Socket> socket);

protected:
  virtual void DoDispose(void);
private:
  virtual void StartApplication(void);    // Called at time specified by Start
  virtual void StopApplication(void);     // Called at time specified by Stop

  void CancelEvents();

  void StartSending();
  void StopSending();
  void SendPacket();

  Ptr<Socket>     m_socket;       //!< Associated socket
  Address         m_peer;         //!< Peer address
  bool            m_connected;    //!< True if connected
  Ptr<RandomVariableStream>  m_onTime;       //!< rng for On Time
  Ptr<RandomVariableStream>  m_offTime;      //!< rng for Off Time
  DataRate        m_cbrRate;      //!< Rate that data is generated
  DataRate        m_cbrRateFailSafe;      //!< Rate that data is generated (check copy)
  uint32_t        m_pktSize;      //!< Size of packets
  uint32_t        m_residualBits; //!< Number of generated, but not sent, bits
  Time            m_lastStartTime; //!< Time last packet sent
  uint64_t        m_maxBytes;     //!< Limit total number of bytes sent
  uint64_t        m_totBytes;     //!< Total bytes sent so far
  EventId         m_startStopEvent;     //!< Event id for next start or stop event
  EventId         m_sendEvent;    //!< Event id of pending "send packet" event
  TypeId          m_tid;          //!< Type of the socket used

  //NIWA
  bool            m_bulksend;     //!< Flag of bulk send
  uint64_t        m_totalRx;
  std::map<Address, Ptr<Packet>> buff;
  Ipv4Address m_clientAddress;
  Address m_actuator;

  int m_totalPacket;

  Ptr<Packet> CreatePacket(uint32_t pktSize, Address peer);

  /// Traced Callback: transmitted packets.
  TracedCallback<Ptr<const Packet>> m_txTrace;
  TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;

private:
  std::string CreateData(Address addr);

private:
  void ScheduleNextTx();
  void ScheduleStartEvent();
  void ConnectionSucceeded(Ptr<Socket> socket);
  void ConnectionFailed(Ptr<Socket> socket);
  void HandleReceive(Ptr<Socket> socket);
};

}

#endif /* MY_ONOFF_APPLICATION_H */
