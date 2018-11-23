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
#include "ns3/flow-monitor-helper.h"
#include "ns3/gtk-config-store.h"
#include "ns3/node-list.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScriptExample");

//Grobal
static const double SIM_TIME = 360.0;
static const int MTU = 1500;

//functions
static void RxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address& address);
static void TxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet);
std::vector<int> stringSplitToInt(const std::string &str, char sep);
std::vector<std::string> stringSplit(const std::string &str, char sep);

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

int arrayProduct(const std::vector<int> array){
  int product = 1;
  for(int i: array){
    product *= i;
  }
  return product;
}

int arrayProductSum(const std::vector<int> array){
  int sum = 0;
  int product = 1;
  for(int i: array){
    product *= i;
    sum += product;
  }
  return sum;
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

/*
static void
ServiceTimeTracer(Ptr<OutputStreamWrapper> stream, const Time& time){
  *stream->GetStream() << Simulator::Now().GetNanoSeconds() << " " << time << std::endl;
}
*/

int
main(int argc, char *argv[])
{
  GtkConfigStore config;

  std::string nodeNum = "1-1-5-20";
  std::string bands = "40Gbps-10Gbps-1Gbps";
  std::string delays = "10ms-5ms-2ms";
  uint16_t sinkPort = 8080;

  CommandLine cmd;
  cmd.AddValue ("node", "the number of nodes of each layer (\"Cloud Server ... Router ... GW\") (ex. \"1-2-10\")", nodeNum);
  cmd.AddValue ("net", "the bandwidth of net of each layer (\"Cloud-Server Server-Router ... Router-GW\") (ex. \"40Gbps-10Gbps-1Gbps\")", bands);
  cmd.AddValue ("delay", "the delay of net of each layer (\"Cloud-Server Server-Router ... Router-GW\") (ex. \"10ms-5ms-1ms\")", delays);
  cmd.Parse(argc, argv);

  const std::vector<int> NODE_NUM = stringSplitToInt(nodeNum, '-');
  const int END_NODES = arrayProduct(NODE_NUM);
  const int ALL_NODES = arrayProductSum(NODE_NUM);
  const int LAYER = NODE_NUM.size();
  const int NET_LAYER = LAYER-1;
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

  NodeContainer allNodes;
  allNodes.Create(ALL_NODES);

  InternetStackHelper internet;
  internet.SetIpv6StackInstall(false);
  internet.InstallAll();


  Ipv4AddressHelper address;
  address.SetBase("10.0.1.0", "255.255.255.252");

  std::ofstream ifs("myExample-NodeList.csv");
  ifs<<"fromN fromA toN toA"<<std::endl;

  NetDeviceContainer devices[ALL_NODES-1];
  Ipv4InterfaceContainer interfaces[ALL_NODES-1];
  int devicesIndex = 0;
  int fromNodeId = 0;
  int toNodeId = 1;
  for(int i=0; i<NET_LAYER; i++){
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(BANDS[i]));
    p2p.SetChannelAttribute("Delay", StringValue(DELAYS[i]));
    for(int j=0; j<(i==0 ? 1: NODE_NUM[i-1]); j++){
      for(int k=0; k<NODE_NUM[i]; k++){
        for(int l=0; l<NODE_NUM[i+1]; l++){
          std::cout<<devicesIndex<<" "<<fromNodeId<<" "<<toNodeId<<std::endl;
          devices[devicesIndex] = p2p.Install(allNodes.Get(fromNodeId),allNodes.Get(toNodeId));
          interfaces[devicesIndex] = address.Assign(devices[devicesIndex]);
          ifs <<allNodes.Get(fromNodeId)->GetId()<<" "<<InetSocketAddress(interfaces[devicesIndex].GetAddress(0), sinkPort).GetIpv4()<<" "
              <<allNodes.Get(toNodeId)->GetId()<<" "<<InetSocketAddress(interfaces[devicesIndex].GetAddress(1), sinkPort).GetIpv4()<<std::endl;
          devicesIndex++;
          toNodeId++;
          address.NewNetwork();
        }
        fromNodeId++;
      }
    }
  }
  ifs.close();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  std::string protocol = "ns3::TcpSocketFactory";

  MyTcpServerHelper myTcpServerHelper(protocol, 5096, 1000.0, InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = myTcpServerHelper.Install(allNodes.Get(0));
  sinkApps.Start(Seconds(0.1));
  sinkApps.Stop(Seconds(SIM_TIME+5));

  MyOnOffHelper clientHelper(protocol, Address());
  AddressValue remoteAddress(InetSocketAddress(interfaces[0].GetAddress(0), sinkPort));
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=1]"));
  //clientHelper.SetConstantRate(DataRateValue("100Gb/p"), UintegerValue(5096));
  clientHelper.SetAttribute("PacketSize", UintegerValue(5096));
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mb/s")));
  clientHelper.SetAttribute("Remote",remoteAddress);
  std::vector<Ptr<Socket>> ns3TcpSockets;
  ApplicationContainer clientApps[END_NODES];
  for(int i=0; i<END_NODES; i++){
    int endNodeId = ALL_NODES-END_NODES+i;
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(allNodes.Get(endNodeId), TcpSocketFactory::GetTypeId());
    ApplicationContainer clientApp;
    clientApps[i] = ApplicationContainer(clientHelper.Install(allNodes.Get(endNodeId)));
    clientApps[i].Get(0)->GetObject<MyOnOffApplication>()->SetSocket(ns3TcpSocket);
    clientApps[i].Start(Seconds(1.0));
    clientApps[i].Stop(Seconds(SIM_TIME));
    ns3TcpSockets.push_back(ns3TcpSocket);
 }

  AsciiTraceHelper asciiTraceHelper;

  for(int i=0;i<END_NODES;i++){
    int endNodeId = ALL_NODES-END_NODES+i;
    std::stringstream txFile;
    txFile << "myExample-Tx-" << allNodes.Get(endNodeId)->GetId() << ".csv";
    std::stringstream txPath;
    txPath << "/NodeList/" << allNodes.Get(endNodeId)->GetId() << "/ApplicationList/*/$ns3::MyOnOffApplication/Tx";
    Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream(txFile.str().c_str());
    Config::ConnectWithoutContext(txPath.str().c_str(),MakeBoundCallback(&TxTracer, txStream));
    std::stringstream rxFile;
    rxFile << "myExample-Rx-" << allNodes.Get(endNodeId)->GetId() << ".csv";
    std::stringstream rxPath;
    rxPath << "/NodeList/" << allNodes.Get(endNodeId)->GetId() << "/ApplicationList/*/$ns3::MyOnOffApplication/Rx";
    Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream(rxFile.str().c_str());
    Config::ConnectWithoutContext(rxPath.str().c_str(),MakeBoundCallback(&RxTracer, rxStream));
  }

  Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream("myExample-RxServer.csv");
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::MyTcpServer/Rx", MakeBoundCallback(&RxTracer, rxStream));
  Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream("myExample-TxServer.csv");
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::MyTcpServer/Tx", MakeBoundCallback(&TxTracer, txStream));

  Simulator::Stop(Seconds(SIM_TIME+5));
  //config.ConfigureAttributes();
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
