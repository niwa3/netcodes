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

// Implement an object to create a star topology.

#include <cmath>
#include <iostream>
#include <sstream>
#include <cmath>

// ns3 includes
#include "ns3/log.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include "ns3/my-onoff-application-helper.h"
#include "ns3/my-receive-server-helper.h"
#include "ns3/my-tree.h"
#include "my-orchestrator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyOrchestrator");

MyOrchestrator::MyOrchestrator(PointToPointTreeHelper p2pHelper)
  : m_simTime(180),
    m_sinkPort(8080),
    m_protocol("ns3::TcpSocketFactory"),
    m_currentServerNum(0),
    m_clientOffTime("ns3::ExponentialRandomVariable[Mean=1]"),
    m_clientPktSize(5096),
    m_clientDataRate("1Mb/s"),
    m_firstServer(0)
{
    m_p2pHelper = p2pHelper;
}

MyOrchestrator::~MyOrchestrator ()
{
}

void MyOrchestrator::Assign(){
  Algorithm();
}

uint32_t MyOrchestrator::AddServerHelper(double meanCalctime, Ipv4Address allowAddress){
  MyTcpServerHelper myTcpServerHelper(m_protocol, m_clientPktSize, meanCalctime, InetSocketAddress(allowAddress,m_sinkPort));
  m_serverHelper[m_currentServerNum] = myTcpServerHelper;
  m_currentServerNum++;
  return m_currentServerNum-1;
}

uint32_t MyOrchestrator::GetCurrentNServer(){
  return m_currentServerNum;
}

void MyOrchestrator::CreateChaine(uint32_t fromServerIndex, uint32_t toServerIndex){
  m_chaine[fromServerIndex] = toServerIndex;
}

void MyOrchestrator::SetSimulationTime(int time){
  m_simTime = time;
}

void MyOrchestrator::SetSinkPort(int port){
  m_sinkPort = port;
}

void MyOrchestrator::SetClientOffTime(std::string offtime){
  m_clientOffTime = offtime;
}

void MyOrchestrator::SetClientPktSize(uint32_t pktSize){
  m_clientPktSize = pktSize;
}

void MyOrchestrator::AssignClient(){
  MyOnOffHelper clientHelper(m_protocol, Address());
  clientHelper.SetAttribute("OffTime", StringValue(m_clientOffTime));
  clientHelper.SetAttribute("PacketSize", UintegerValue(m_clientPktSize));
  MyReceiveServerHelper serverHelper(m_protocol, m_clientPktSize, InetSocketAddress(Ipv4Address::GetAny(), m_sinkPort));

  for(size_t i=0;i<m_p2pHelper.GetNGroups(m_p2pHelper.GetNLayers()-1);i++){
    for(size_t j=0;j<m_p2pHelper.GetNNodes(m_p2pHelper.GetNLayers()-1,i);j++){
      AddressValue remoteAddress(InetSocketAddress(m_p2pHelper.GetParentAddress(m_p2pHelper.GetNLayers()-1,i,j,m_firstServer), m_sinkPort));
      clientHelper.SetAttribute("Remote",remoteAddress);
      AddressValue actuator(InetSocketAddress(m_p2pHelper.GetIpv4Address(m_p2pHelper.GetNLayers()-1,i,j,1), m_sinkPort));
      clientHelper.SetAttribute("Actuator",actuator);
      ApplicationContainer clientApp;
      clientApp.Add(clientHelper.Install(m_p2pHelper.GetNode(m_p2pHelper.GetNLayers()-1,i,j)));
      ApplicationContainer serverApp;
      serverApp.Add(serverHelper.Install(m_p2pHelper.GetNode(m_p2pHelper.GetNLayers()-1,i,j)));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(m_simTime));
      serverApp.Start(Seconds(1.0));
      serverApp.Stop(Seconds(m_simTime+10));
    }
  }
}

void MyOrchestrator::AssignServer(uint32_t serverIndex, uint32_t nLayer){
  for(size_t i=0;i<m_p2pHelper.GetNGroups(nLayer);i++){
    for(size_t j=0;j<m_p2pHelper.GetNNodes(nLayer,i);j++){
      auto chaine = m_chaine.find(serverIndex);
      if(chaine != m_chaine.end()){
        AddressValue nextService = AddressValue(InetSocketAddress(m_p2pHelper.GetParentAddress(nLayer,i,j,m_serverPlace[m_chaine[serverIndex]]),m_sinkPort));
        m_serverHelper[serverIndex].SetAttribute("NextService", nextService);
        std::stringstream meanTime;
        meanTime << "ns3::ExponentialRandomVariable[Mean=" << 100.0*(nLayer+1) << "]";
        m_serverHelper[serverIndex].SetAttribute("CalcTime", StringValue(meanTime.str()));
      }
      ApplicationContainer servers = m_p2pHelper.InstallApp(m_serverHelper[serverIndex], nLayer, i, j);
      servers.Start(Seconds(0.1));
      servers.Stop(Seconds(m_simTime+5));
    }
  }
}

double MyOrchestrator::GetAccessDelay(uint32_t pktSize, DataRate bw, double lambda, double mu){
  double rho = lambda/mu;
  double t = 1/(mu*(1-rho));
  return t;
}

double MyOrchestrator::GetBottleDelay(uint32_t pktSize, DataRate bbw, DataRate abw, double lambda, double mu){
  uint32_t m = (bbw*Seconds(1))/(abw*Seconds(1));
  double rho = lambda/mu;
  double t = 1.0*((GetErlangC(bbw,abw,rho,mu)/(mu*(1.0-rho)))*(1.0-((bbw*Seconds(1))/(abw*Seconds(1))-m)*(1.0-rho)));
  return t;
}

double MyOrchestrator::GetProcessDelay(double lambda, double mu){
  double rho = lambda/mu;
  double t = rho/((1-rho)*lambda);
  return t;
}

double MyOrchestrator::GetErlangC(DataRate bbw, DataRate abw, double rho, double mu){
  uint32_t m = (bbw*Seconds(1))/(abw*Seconds(1));
  double a = (bbw*Seconds(1))*rho/(abw*Seconds(1));
  double l = (std::pow(a,m)/Factorial(m))*(1/(1-rho));
  double sum = 0;
  for(uint32_t i=0; i<=m; i++){
    sum += (static_cast<double>(std::pow(a,i))/static_cast<double>(Factorial(i)));
  }
  double erlang = l/(sum+l);
  return erlang;
}

uint32_t MyOrchestrator::Factorial(uint32_t m){
  uint32_t sum = 1;
  for(uint32_t i=1; i<=m; i++){
    sum *= i;
  }
  return sum;
}

void MyOrchestrator::Algorithm(){
  AssignServer(0,0);
  AssignServer(1,2);
  m_firstServer = 2;
  AssignClient();
}

} // namespace ns3
