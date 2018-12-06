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

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/my-onoff-application-helper.h"
#include "ns3/my-tcp-server-helper.h"
#include "ns3/my-receive-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gtk-config-store.h"

#include "ns3/my-tree.h"
#include "ns3/my-orchestrator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScriptExample");

//Grobal
static const double SIM_TIME = 180.0;
static const int MTU = 1500;

//My functions start
std::vector<int> stringSplitToInt(const std::string &str, char sep)
{
  std::vector<int> v;
  std::stringstream ss(str);
  std::string buffer;
  while( getline(ss, buffer, sep) ) {
    v.push_back(std::atoi(buffer.c_str()));
  }
  return v;
}

std::vector<std::string> stringSplit(const std::string &str, char sep)
{
  std::vector<std::string> v;
  std::stringstream ss(str);
  std::string buffer;
  while(getline(ss, buffer, sep)) {
    v.push_back(buffer);
  }
  return v;
}

static void
RxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address& address)
{
  *stream->GetStream() << InetSocketAddress::ConvertFrom(address).GetIpv4() << " " << Simulator::Now().GetNanoSeconds() << " " << packet->GetSize() << std::endl;
}

static void
TxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  *stream->GetStream() << Simulator::Now().GetNanoSeconds() << " " << packet->GetSize() << std::endl;
}

int
main(int argc, char *argv[])
{
  GtkConfigStore config;

  //uint16_t sinkPort = 8080;

  std::string nodeNum = "1-1-5-20";
  std::string bands = "40Gbps-10Gbps-1Gbps";
  std::string delays = "10ms-5ms-2ms";
  uint32_t midServer = 1;

  CommandLine cmd;
  cmd.AddValue ("node", "the number of nodes of each layer (\"Cloud Server ... Router ... GW\") (ex. \"1-2-10\")", nodeNum);
  cmd.AddValue ("net", "the bandwidth of net of each layer (\"Cloud-Server Server-Router ... Router-GW\") (ex. \"40Gbps-10Gbps-1Gbps\")", bands);
  cmd.AddValue ("delay", "the delay of net of each layer (\"Cloud-Server Server-Router ... Router-GW\") (ex. \"10ms-5ms-1ms\")", delays);
  cmd.AddValue ("midServer", "location of middle server (ex. \"1\")", midServer);
  cmd.Parse(argc, argv);

  const std::vector<int> NODE_NUM = stringSplitToInt(nodeNum, '-');
  const std::vector<std::string> BANDS = stringSplit(bands, '-');
  const std::vector<std::string> DELAYS = stringSplit(delays, '-');

  NS_LOG_INFO("Creating Topology");

  Header* temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("TCP Header size is: " << tcp_header);
  delete temp_header;
  const int MSS = MTU - 20 - (ip_header + tcp_header);

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(MSS));

  PointToPointTreeHelper p2ptree(NODE_NUM,BANDS,DELAYS);

  InternetStackHelper internet;
  internet.SetIpv6StackInstall(false);
  p2ptree.InstallStack(internet);

  Ipv4AddressHelper address;
  address.SetBase("10.0.1.0", "255.255.255.252");

  p2ptree.AssignIpv4Addresses(address);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  //std::string protocol = "ns3::TcpSocketFactory";

  MyOrchestrator orch(p2ptree);
  orch.SetClientOffTime("ns3::ExponentialRandomVariable[Mean=200000]");
  uint8_t first = orch.AddServerHelper(100.0,Ipv4Address::GetAny());
  uint8_t second = orch.AddServerHelper(100.0,Ipv4Address::GetAny());
  orch.CreateChaine(second, first);
  orch.Assign();

  AsciiTraceHelper asciiTraceHelper;

  {
    int i = 0;
    for(size_t j=0;j<p2ptree.GetNNodes(p2ptree.GetNLayers()-1,i);j++){
      uint32_t id = p2ptree.GetNode(p2ptree.GetNLayers()-1,i,j)->GetId();
      std::stringstream txFile;
      txFile << "myExampleTx-" << id << ".csv";
      std::stringstream txPath;
      txPath << "/NodeList/" << id << "/ApplicationList/*/$ns3::MyOnOffApplication/Tx";
      Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream(txFile.str().c_str());
      Config::ConnectWithoutContext(txPath.str().c_str(),MakeBoundCallback(&TxTracer, txStream));
      std::stringstream rxFile;
      rxFile << "myExampleRx-" << id << ".csv";
      std::stringstream rxPath;
      rxPath << "/NodeList/" << id << "/ApplicationList/*/$ns3::MyReceiveServer/Rx";
      Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream(rxFile.str().c_str());
      Config::ConnectWithoutContext(rxPath.str().c_str(),MakeBoundCallback(&RxTracer, rxStream));
    }
  }

  {
    int i = 0;
    int j = 0;
    uint32_t id = p2ptree.GetNode(midServer,i,j)->GetId();
    std::stringstream txFile;
    txFile << "myExampleTxMidServer-" << id << ".csv";
    std::stringstream txPath;
    txPath << "/NodeList/" << id << "/ApplicationList/*/$ns3::MyTcpServer/Tx";
    Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream(txFile.str().c_str());
    Config::ConnectWithoutContext (txPath.str().c_str(), MakeBoundCallback(&TxTracer, txStream));
    std::stringstream rxFile;
    rxFile << "myExampleRxMidServer-" << id << ".csv";
    std::stringstream rxPath;
    rxPath << "/NodeList/" << id << "/ApplicationList/*/$ns3::MyTcpServer/Rx";
    Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream(rxFile.str().c_str());
    Config::ConnectWithoutContext (rxPath.str().c_str(), MakeBoundCallback(&RxTracer, rxStream));
  }

  Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream("myExampleRxServer.csv");
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$ns3::MyTcpServer/Rx", MakeBoundCallback(&RxTracer, rxStream));
  Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream("myExampleTxServer.csv");
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$ns3::MyTcpServer/Tx", MakeBoundCallback(&TxTracer, txStream));

  Simulator::Stop(Seconds(SIM_TIME+10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
