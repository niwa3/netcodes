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

int
main(int argc, char *argv[])
{
  GtkConfigStore config;

  std::string nodeNum = "1-1-5-20";
  std::string bands = "40Gbps-10Gbps-1Gbps";
  std::string delays = "10ms-5ms-2ms";
  uint32_t top = 0;
  uint32_t mid = 0;
  uint32_t end = 0;
  std::string path="/root/result";

  CommandLine cmd;
  cmd.AddValue ("node", "the number of nodes of each layer (\"Cloud Server ... Router ... GW\") (ex. \"1-2-10\")", nodeNum);
  cmd.AddValue ("net", "the bandwidth of net of each layer (\"Cloud-Server Server-Router ... Router-GW\") (ex. \"40Gbps-10Gbps-1Gbps\")", bands);
  cmd.AddValue ("delay", "the delay of net of each layer (\"Cloud-Server Server-Router ... Router-GW\") (ex. \"10ms-5ms-1ms\")", delays);
  cmd.AddValue ("top", "top server place (ex. 0)", top);
  cmd.AddValue ("mid", "mid server place (ex. 1)", mid);
  cmd.AddValue ("end", "end server place (ex. 1)", end);
  cmd.AddValue ("path", "path of trace file (ex. /root/result)", mid);
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

  MyOrchestrator orch(p2ptree);
  orch.SetClientOffTime("ns3::ExponentialRandomVariable[Mean=200000]");
  uint8_t first = orch.AddServerHelper(100.0,Ipv4Address::GetAny());
  uint8_t second = orch.AddServerHelper(100.0,Ipv4Address::GetAny());
  uint8_t third = orch.AddServerHelper(100.0,Ipv4Address::GetAny());
  orch.CreateChaine(second, first);
  orch.CreateChaine(third, second);
  orch.SetPlace(top,mid,end);
  orch.SetPath(path);
  orch.Assign();

  Simulator::Stop(Seconds(SIM_TIME+10));
  config.ConfigureAttributes();
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
