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
  std::map<uint32_t, MyTcpServerHelper> m_serverHelper;
  std::map<uint32_t, uint32_t> m_serverPlace;
  std::map<uint32_t, uint32_t> m_chaine;
  std::vector<uint32_t> m_processCount;

  int m_simTime;
  int m_sinkPort;
  std::string m_protocol;

  uint32_t m_currentServerNum;
  std::string m_clientOffTime;
  uint32_t m_clientPktSize;
  std::string m_clientDataRate;
  uint32_t m_firstServer;

  std::vector<uint32_t> m_place;

  std::vector<std::vector<double>> m_process;

  std::string m_path;

public:
  void Assign();
  uint32_t AddServerHelper(std::vector<double> meanCalctime, Ipv4Address allowAddress);
  uint32_t GetCurrentNServer();
  void CreateChaine(uint32_t fromServerIndex, uint32_t toServerIndex);
  void SetSimulationTime(int time);
  void SetSinkPort(int port);
  void SetClientOffTime(std::string offtime);
  void SetClientPktSize(uint32_t pktSize);
  void SetPlace(std::vector<uint32_t> place);
  void SetPath(std::string path);

private:
  void AssignClient();
  void AssignServer(uint32_t serverIndex, uint32_t nLayer);
  double GetAccessDelay(uint32_t pktSize, DataRate bw, double lambda, double mu);
  double GetBottleDelay(uint32_t pktSize, DataRate bbw, DataRate abw, double lambda, double mu);
  double GetProcessDelay(double lambda, double mu);
  double GetErlangC(DataRate bbw, DataRate abw, double rho, double mu);
  uint32_t Factorial(uint32_t m);
  void Algorithm();
  void SetTracer();
  void AddProcessCount(uint32_t place);

};

} // namespace ns3

#endif
