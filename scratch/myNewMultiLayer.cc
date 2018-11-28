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

#include "ns3/my-tree.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScriptExample");

//Grobal
static const double SIM_TIME = 360.0;
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
  //const int END_NODES = arrayProduct(NODE_NUM);
  //const int ALL_NODES = arrayProductSum(NODE_NUM);
  //const int LAYER = NODE_NUM.size();
  //const int NET_LAYER = LAYER-1;
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

  std::string protocol = "ns3::TcpSocketFactory";

  MyTcpServerHelper myTcpServerHelper(protocol, 5096, 100.0, InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = myTcpServerHelper.Install(p2ptree.GetNode(0,0,0));
  sinkApps.Start(Seconds(0.1));
  sinkApps.Stop(Seconds(SIM_TIME+5));

  //MyTcpServerHelper secondServerHelper(protocol, 5096, 100.0, InetSocketAddress(Ipv4Address::GetAny(), sinkPort), InetSocketAddress(p2ptree.GetIpv4Address(0,0,0,1) , sinkPort));
  //ApplicationContainer secondApps = secondServerHelper.Install(p2ptree.GetNode(1,0,0));
  //secondApps.Start(Seconds(0.1));
  //secondApps.Stop(Seconds(SIM_TIME+5));

  MyOnOffHelper clientHelper(protocol, Address());
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=1]"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(5096));
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mb/s")));
  MyReceiveServerHelper serverHelper(protocol, 5096, InetSocketAddress(Ipv4Address::GetAny(), sinkPort));

  std::vector<Ptr<Socket>> ns3TcpSockets;
  for(size_t i=0;i<p2ptree.GetNGroups(p2ptree.GetNLayers()-1);i++){
    for(size_t j=0;j<p2ptree.GetNNodes(p2ptree.GetNLayers()-1,i);j++){
      //AddressValue remoteAddress(InetSocketAddress(p2ptree.GetIpv4Address(1,0,0,j+2), sinkPort));
      AddressValue remoteAddress(InetSocketAddress(p2ptree.GetIpv4Address(0,0,0,1), sinkPort));
      clientHelper.SetAttribute("Remote",remoteAddress);

      AddressValue actuator(InetSocketAddress(p2ptree.GetIpv4Address(p2ptree.GetNLayers()-1,i,j,1), sinkPort));
      clientHelper.SetAttribute("Actuator",actuator);

      ApplicationContainer clientApp;
      clientApp.Add(clientHelper.Install(p2ptree.GetNode(p2ptree.GetNLayers()-1,i,j)));
      ApplicationContainer serverApp;
      clientApp.Add(serverHelper.Install(p2ptree.GetNode(p2ptree.GetNLayers()-1,i,j)));

      Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(p2ptree.GetNode(p2ptree.GetNLayers()-1,i,j), TcpSocketFactory::GetTypeId());
      clientApp.Get(0)->GetObject<MyOnOffApplication>()->SetSocket(ns3TcpSocket);

      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(SIM_TIME));
      serverApp.Start(Seconds(1.0));
      serverApp.Stop(Seconds(SIM_TIME+10));

      ns3TcpSockets.push_back(ns3TcpSocket);
    }
  }

  AsciiTraceHelper asciiTraceHelper;

  for(size_t i=0;i<p2ptree.GetNGroups(p2ptree.GetNLayers()-1);i++){
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

  Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream("myExampleRxServer.csv");
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$ns3::MyTcpServer/Rx", MakeBoundCallback(&RxTracer, rxStream));
  Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream("myExampleTxServer.csv");
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$ns3::MyTcpServer/Tx", MakeBoundCallback(&TxTracer, txStream));
  Ptr<OutputStreamWrapper> rxStream2 = asciiTraceHelper.CreateFileStream("myExampleRxMidServer.csv");
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/*/$ns3::MyTcpServer/Rx", MakeBoundCallback(&RxTracer, rxStream2));
  Ptr<OutputStreamWrapper> txStream2 = asciiTraceHelper.CreateFileStream("myExampleTxMidServer.csv");
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/*/$ns3::MyTcpServer/Tx", MakeBoundCallback(&TxTracer, txStream2));

  Simulator::Stop(Seconds(SIM_TIME+5));
  //config.ConfigureAttributes();
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
