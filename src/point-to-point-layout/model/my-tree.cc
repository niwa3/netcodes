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

// ns3 includes
#include "ns3/log.h"
#include "ns3/constant-position-mobility-model.h"

#include "ns3/node-list.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/vector.h"
#include "ns3/ipv6-address-generator.h"

#include "ns3/core-module.h"

#include "my-tree.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointTreeHelper");

PointToPointTreeHelper::PointToPointTreeHelper(std::vector<int> nodeNums, std::vector<std::string> bandwidth, std::vector<std::string> delays)
{
  m_nodeNums = nodeNums;
  m_bandwidths = bandwidth;
  m_delays = delays;

  NodeContainer top;
  top.Create(1);
  std::vector<NodeContainer> topLayer;
  topLayer.push_back(top);
  m_layers.push_back(topLayer);
  for(size_t i=1;i<m_nodeNums.size();i++){
    std::vector<NodeContainer> layer;
    for(size_t j=0;j<m_layers[i-1].size()*m_nodeNums[i-1];j++){
      NodeContainer nodes;
      nodes.Create(m_nodeNums[i]);
      layer.push_back(nodes);
    }
    m_layers.push_back(layer);
  }
}

PointToPointTreeHelper::~PointToPointTreeHelper ()
{
}

Ptr<Node>
PointToPointTreeHelper::GetNode (uint32_t nLayer, uint32_t nGroup, uint32_t nNode) const
{
  return m_layers[nLayer][nGroup].Get(nNode);
}

Ipv4Address
PointToPointTreeHelper::GetIpv4Address (uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t nInterface) const
{
  return m_layers[nLayer][nGroup].Get(nNode)->GetObject<Ipv4>()->GetAddress(nInterface,0).GetLocal();
}

void 
PointToPointTreeHelper::InstallStack (InternetStackHelper stack)
{
  stack.InstallAll();
}

void 
PointToPointTreeHelper::AssignIpv4Addresses (Ipv4AddressHelper address)
{
  std::ofstream ifs("myExampleNodeList.csv");
  ifs<<"fromN fromA toN toA"<<std::endl;

  for(size_t nLayer=0; nLayer<(m_layers.size()-1); nLayer++){
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(m_bandwidths[nLayer]));
    p2p.SetChannelAttribute("Delay", StringValue(m_delays[nLayer]));
    for(size_t nGroup=0; nGroup<m_layers[nLayer].size(); nGroup++){
      for(size_t nNode=0; nNode<m_layers[nLayer][nGroup].GetN(); nNode++){
        for(int i=0; i<m_nodeNums[nLayer+1]; i++){
          Ptr<Node> fromNode = GetNode(nLayer, nGroup, nNode);
          Ptr<Node> toNode = GetNode(nLayer+1, nGroup*m_layers[nLayer][nGroup].GetN()+nNode, i);
          NetDeviceContainer device = p2p.Install(fromNode,toNode);
          Ipv4InterfaceContainer interface = address.Assign(device);
          ifs <<fromNode->GetId()<<" "<<InetSocketAddress(interface.GetAddress(0)).GetIpv4()<<" "
              <<toNode->GetId()<<" "<<InetSocketAddress(interface.GetAddress(1)).GetIpv4()<<std::endl;
          address.NewNetwork();
          m_devices.push_back(device);
          m_interfaces.push_back(interface);
        }
      }
    }
  }
  ifs.close();
}

size_t PointToPointTreeHelper::GetNLayers(){
  return m_layers.size();
}

size_t PointToPointTreeHelper::GetNGroups(uint32_t nLayer){
  return m_layers[nLayer].size();
}

size_t PointToPointTreeHelper::GetNNodes(uint32_t nLayer, uint32_t nGroup){
  return m_layers[nLayer][nGroup].GetN();
}

Ipv4Address PointToPointTreeHelper::GetPairentAddress(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t pairentLayer){
  return GetIpv4Address(nLayer, nGroup, nNode, nNode);
}
} // namespace ns3
