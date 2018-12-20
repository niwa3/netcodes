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
#include "ns3/json.h"
#include "my-orchestrator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyOrchestrator");

static void
RxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address& address)
{
  std::string err;
  uint8_t buf[5120];
  packet->CopyData(buf, 5120);
  std::stringstream text;
  text << buf;
  //NS_LOG_DEBUG(text.str());
  auto json = json11::Json::parse(text.str(), err);
  int total = json["Total"].int_value();
  *stream->GetStream() << InetSocketAddress::ConvertFrom(address).GetIpv4() << " " << Simulator::Now().GetNanoSeconds() << " " << packet->GetSize() << " " << total << std::endl;
}

static void
TxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  std::string err;
  uint8_t buf[5120];
  packet->CopyData(buf, 5120);
  std::stringstream text;
  text << buf;
  //NS_LOG_DEBUG(text.str());
  auto json = json11::Json::parse(text.str(), err);
  int total = json["Total"].int_value();
  *stream->GetStream() << Simulator::Now().GetNanoSeconds() << " " << packet->GetSize() << " " << total << std::endl;
}

static void
QueueTracer(Ptr<OutputStreamWrapper> stream, uint32_t packetsInQueueOld, uint32_t packetsInQueueNew)
{
  if(packetsInQueueOld<packetsInQueueNew){
    *stream->GetStream() << Simulator::Now().GetNanoSeconds() << " " << packetsInQueueOld << " " << packetsInQueueNew << std::endl;
  }
}


MyOrchestrator::MyOrchestrator(PointToPointTreeHelper p2pHelper)
  : m_simTime(180),
    m_sinkPort(8080),
    m_protocol("ns3::TcpSocketFactory"),
    m_currentServerNum(0),
    m_clientOffTime("ns3::ExponentialRandomVariable[Mean=1]"),
    m_clientPktSize(5120),
    m_clientDataRate("1Mb/s"),
    m_firstServer(0)
{
  m_p2pHelper = p2pHelper;
  for(size_t i=0;i<p2pHelper.GetNLayers();i++){
    m_processCount.push_back(0);
  }
}

MyOrchestrator::~MyOrchestrator ()
{
}

void MyOrchestrator::Assign(){
  Algorithm();
  SetTracer();
}

uint32_t MyOrchestrator::AddServerHelper(std::vector<double> meanCalctime, Ipv4Address allowAddress){
  MyTcpServerHelper myTcpServerHelper(m_protocol, m_clientPktSize, meanCalctime[0], InetSocketAddress(allowAddress,m_sinkPort+m_currentServerNum));
  m_serverHelper[m_currentServerNum] = myTcpServerHelper;
  m_process.push_back(meanCalctime);
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
      AddressValue remoteAddress(InetSocketAddress(m_p2pHelper.GetParentAddress(m_p2pHelper.GetNLayers()-1,i,j,m_firstServer), m_sinkPort+m_currentServerNum-1));
      clientHelper.SetAttribute("Remote",remoteAddress);
      AddressValue actuator(InetSocketAddress(m_p2pHelper.GetIpv4Address(m_p2pHelper.GetNLayers()-1,i,j,1), m_sinkPort));
      clientHelper.SetAttribute("Actuator",actuator);
      ApplicationContainer clientApp;
      clientApp.Add(clientHelper.Install(m_p2pHelper.GetNode(m_p2pHelper.GetNLayers()-1,i,j)));
      ApplicationContainer serverApp;
      serverApp.Add(serverHelper.Install(m_p2pHelper.GetNode(m_p2pHelper.GetNLayers()-1,i,j)));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(m_simTime));
      serverApp.Start(Seconds(0.1));
      serverApp.Stop(Seconds(m_simTime+10));
    }
  }
}

void MyOrchestrator::AssignServer(uint32_t serverIndex, uint32_t nLayer){
  for(size_t i=0;i<m_p2pHelper.GetNGroups(nLayer);i++){
    for(size_t j=0;j<m_p2pHelper.GetNNodes(nLayer,i);j++){
      auto chaine = m_chaine.find(serverIndex);
      if(chaine != m_chaine.end()){
        std::map<Address, Address> addrTable;
        if(m_serverPlace[m_chaine[serverIndex]]<nLayer){
          Address nextService = InetSocketAddress(m_p2pHelper.GetParentAddress(nLayer,i,j,m_serverPlace[m_chaine[serverIndex]]),m_sinkPort+m_chaine[serverIndex]);
          std::vector<Ipv4Address> children = m_p2pHelper.GetChildrenAddress(nLayer,i,j,m_p2pHelper.GetNLayers()-1);
          for(Ipv4Address k: children){
            //NS_LOG_DEBUG("MyOrchestrator >> make addrTable "<<InetSocketAddress(k));
            addrTable[InetSocketAddress(k)] = nextService;
          }
        }
        else if(m_serverPlace[m_chaine[serverIndex]]==nLayer){
          Address nextService = InetSocketAddress(m_p2pHelper.GetIpv4Address(nLayer,i,j,0),m_sinkPort+m_chaine[serverIndex]);
          std::vector<Ipv4Address> children = m_p2pHelper.GetChildrenAddress(nLayer,i,j,m_p2pHelper.GetNLayers()-1);
          for(Ipv4Address k: children){
            //NS_LOG_DEBUG("MyOrchestrator >> make addrTable "<<InetSocketAddress(k));
            addrTable[InetSocketAddress(k)] = nextService;
          }
        }
        else{
          std::vector<uint32_t> nextServiceList = m_p2pHelper.GetChildrenId(nLayer,i,j,m_serverPlace[m_chaine[serverIndex]]);
          for(uint32_t k: nextServiceList){
            std::vector<Ipv4Address> children = m_p2pHelper.GetChildrenAddress(k, m_p2pHelper.GetNLayers()-1-m_serverPlace[m_chaine[serverIndex]]);
            for(Ipv4Address l: children){
              //NS_LOG_DEBUG("MyOrchestrator >> make addrTable from "<<InetSocketAddress(m_p2pHelper.GetIpv4Address(k,1))<<" to "<<InetSocketAddress(l));
              addrTable[InetSocketAddress(l)] = InetSocketAddress(m_p2pHelper.GetIpv4Address(k,1),m_sinkPort+m_chaine[serverIndex]);
            }
          }
        }
        uint32_t nProcess = m_processCount[nLayer];
        std::stringstream meanTime;
        meanTime << "ns3::ExponentialRandomVariable[Mean=" << m_process[serverIndex][nLayer]*nProcess << "]";
        NS_LOG_DEBUG(serverIndex<<":"<<meanTime.str());
        m_serverHelper[serverIndex].SetAttribute("CalcTime", StringValue(meanTime.str()));
        ApplicationContainer servers = m_p2pHelper.InstallApp(m_serverHelper[serverIndex], nLayer, i, j);
        //NS_LOG_DEBUG("address: "<<m_p2pHelper.GetIpv4Address(nLayer,i,j,1)<<" size: "<<addrTable.size());
        servers.Get(0)->GetObject<MyTcpServer>()->SetAddressTable(addrTable);
        servers.Start(Seconds(0.1));
        servers.Stop(Seconds(m_simTime+5));
      }
      else{
        uint32_t nProcess = m_processCount[nLayer];
        std::stringstream meanTime;
        meanTime << "ns3::ExponentialRandomVariable[Mean=" << m_process[serverIndex][nLayer]*nProcess << "]";
        NS_LOG_DEBUG(serverIndex<<":"<<meanTime.str());
        m_serverHelper[serverIndex].SetAttribute("CalcTime", StringValue(meanTime.str()));
        ApplicationContainer servers = m_p2pHelper.InstallApp(m_serverHelper[serverIndex], nLayer, i, j);
        servers.Start(Seconds(0.1));
        servers.Stop(Seconds(m_simTime+5));
      }
    }
  }
  m_serverPlace[serverIndex] = nLayer;
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
  //AssignServer(0,3);
  for(auto i: m_place){
    AddProcessCount(i);
  }
  for(size_t i=(m_place.size()-1); i>=0; i--){
    AssignServer(i,m_place[i]);
  }
  m_firstServer = m_place[0];
  AssignClient();
}

void MyOrchestrator::SetTracer(){
  AsciiTraceHelper asciiTraceHelper;
  {
    int i = 0;
    for(size_t j=0;j<m_p2pHelper.GetNNodes(m_p2pHelper.GetNLayers()-1,i);j++){
      uint32_t id = m_p2pHelper.GetNode(m_p2pHelper.GetNLayers()-1,i,j)->GetId();
      std::stringstream txFile;
      txFile << m_path << "/myEndTx-" << id << ".csv";
      std::stringstream txPath;
      txPath << "/NodeList/" << id << "/ApplicationList/*/$ns3::MyOnOffApplication/Tx";
      Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream(txFile.str().c_str());
      Config::ConnectWithoutContext(txPath.str().c_str(),MakeBoundCallback(&TxTracer, txStream));
      std::stringstream rxFile;
      rxFile << m_path << "/myEndRx-" << id << ".csv";
      std::stringstream rxPath;
      rxPath << "/NodeList/" << id << "/ApplicationList/*/$ns3::MyReceiveServer/Rx";
      Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream(rxFile.str().c_str());
      Config::ConnectWithoutContext(rxPath.str().c_str(),MakeBoundCallback(&RxTracer, rxStream));
      std::stringstream qFile;
      qFile << m_path << "/myQueueLen-" << id << ".csv";
      std::stringstream qPath;
      qPath << "/NodeList/" << id << "/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/PacketsInQueue";
      Ptr<OutputStreamWrapper> qStream = asciiTraceHelper.CreateFileStream(qFile.str().c_str());
      Config::ConnectWithoutContext(qPath.str().c_str(),MakeBoundCallback(&QueueTracer, qStream));
    }
  }

  {
    int i = 0;
    int j = 0;
    std::vector<uint32_t> counter(m_p2pHelper.GetNLayers(),0);
    for(auto k: m_serverPlace){
      uint32_t id = m_p2pHelper.GetNode(k.second,i,j)->GetId();
      std::stringstream txFile;
      txFile << m_path << "/myServer"<<k.first<<"Tx-" << id << ".csv";
      std::stringstream txPath;
      txPath << "/NodeList/" << id << "/ApplicationList/"<<counter[k.second]<<"/$ns3::MyTcpServer/Tx";
      Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream(txFile.str().c_str());
      Config::ConnectWithoutContext (txPath.str().c_str(), MakeBoundCallback(&TxTracer, txStream));
      std::stringstream rxFile;
      rxFile << m_path << "/myServer"<<k.first<<"Rx-" << id << ".csv";
      std::stringstream rxPath;
      rxPath << "/NodeList/" << id << "/ApplicationList/"<<counter[k.second]<<"/$ns3::MyTcpServer/Rx";
      Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream(rxFile.str().c_str());
      Config::ConnectWithoutContext (rxPath.str().c_str(), MakeBoundCallback(&RxTracer, rxStream));
      counter[k.second] += 1;
      std::stringstream qFile;
      qFile << m_path << "/myQueueLen-" << id << ".csv";
      std::stringstream qPath;
      qPath << "/NodeList/" << id << "/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/PacketsInQueue";
      Ptr<OutputStreamWrapper> qStream = asciiTraceHelper.CreateFileStream(qFile.str().c_str());
      Config::ConnectWithoutContext(qPath.str().c_str(),MakeBoundCallback(&QueueTracer, qStream));
    }
  }
}

//void MyOrchestrator::SetPlace(uint32_t top, uint32_t mid, uint32_t end){
void MyOrchestrator::SetPlace(std::vector<uint32_t> place){
  m_place = place;
}

void MyOrchestrator::SetPath(std::string path){
  m_path = path;
}

void MyOrchestrator::AddProcessCount(uint32_t place){
  uint32_t tmp = m_processCount[place];
  m_processCount[place] = tmp+1;
}

} // namespace ns3
