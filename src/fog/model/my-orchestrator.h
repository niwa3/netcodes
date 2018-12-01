/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

// Define an object to create a star topology.

#ifndef MY_ORCHESTRATOR_H
#define MY_ORCHESTRATOR_H

#include <string>
#include <vector>
#include <map>

#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/applications-module.h"
#include "ns3/my-tree.h"
#include "ns3/my-tcp-server-helper.h"

namespace ns3 {

class MyOrchestrator
{
public:
  MyOrchestrator (PointToPointTreeHelper p2pHelper);
  ~MyOrchestrator ();

private:
  PointToPointTreeHelper m_p2pHelper;
  std::map<uint8_t, MyTcpServerHelper> m_serverHelper;
  std::map<uint8_t, uint8_t> m_serverPlace;
  std::map<uint8_t, uint8_t> m_chaine;

  int m_simTime;
  int m_sinkPort;
  std::string m_protocol;

  uint8_t m_currentServerNum;
  std::string m_clientOnTime;
  std::string m_clientOffTime;
  uint32_t m_clientPktSize;
  std::string m_clientDataRate;
  uint32_t m_firstServer;

public:
  template <typename T> void SetAttribute(std::string name, T value);
  void Assign();
  uint8_t AddServerHelper(double meanCalctime, Ipv4Address allowAddress);
  uint8_t GetCurrentNServer();
  void CreateChaine(uint8_t fromServerIndex, uint8_t toServerIndex);

private:
  void Algorithm();
  void AssignClient();
  void AssignServer(uint8_t serverIndex, uint8_t nLayer);

};

template <typename T> void MyOrchestrator::SetAttribute(std::string name, T value){
  if(name=="SimTime"){
    m_simTime = value;
  }
  else if(name=="SinkPort"){
    m_sinkPort = value;
  }
  else if(name=="ClientOnTime"){
    m_clientOnTime = value;
  }
}

} // namespace ns3

#endif
