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
#include <algorithm>

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

LinkContainer::LinkContainer()
  : m_parent(0)
{
}

LinkContainer::LinkContainer(uint32_t parent)
  : m_parent(parent)
{
}

LinkContainer::~LinkContainer(){
}

void LinkContainer::SetParent(uint32_t parent){
  m_parent = parent;
}

void LinkContainer::SetChildren(std::vector<uint32_t> children){
  m_children = children;
}

void LinkContainer::AddChild(uint32_t child){
  m_children.push_back(child);
}

uint32_t LinkContainer::GetParent(){
  return m_parent;
}

uint32_t LinkContainer::GetNthChild(uint32_t child){
  return std::distance(m_children.begin(),std::find(m_children.begin(), m_children.end(), child));
}

size_t LinkContainer::GetNChildren(){
  return m_children.size();
}

uint32_t LinkContainer::GetChild(uint32_t nth){
  return m_children[nth];
}

std::vector<uint32_t> LinkContainer::GetChildren(){
  return m_children;
}

bool LinkContainer::IsChildEmpty(){
  return m_children.empty();
}

LinkContainer LinkContainer::CreateLinkContainer(){
  LinkContainer newContainer;
  return newContainer;
}

LinkContainer LinkContainer::CreateLinkContainer(uint32_t parent){
  LinkContainer newContainer(parent);
  return newContainer;
}

// p2p
PointToPointTreeHelper::PointToPointTreeHelper(){
}

PointToPointTreeHelper::PointToPointTreeHelper(std::vector<int> nodeNums, std::vector<std::string> bandwidth, std::vector<std::string> delays)
{
  m_nodeNums = nodeNums;
  m_bandwidths = bandwidth;
  m_delays = delays;

  NodeContainer top;
  top.Create(1);
  //m_link.push_back(LinkContainer::CreateLinkContainer());
  m_link.emplace_back();

  std::vector<NodeContainer> topLayer;
  topLayer.push_back(top);
  for(size_t i=1;i<m_nodeNums.size();i++){
    std::vector<NodeContainer> layer;
    for(size_t j=0;j<GetNGroups(i-1);j++){
      for(int k=0; k<m_nodeNums[i-1];k++){
        NodeContainer nodes;
        nodes.Create(m_nodeNums[i]);
        for(int l=0; l<m_nodeNums[i]; l++){
          m_link.emplace_back(GetNode(i-1,j,k)->GetId());
          m_link[GetNode(i-1,j,k)->GetId()].AddChild(nodes.Get(l)->GetId());
        }
        layer.push_back(nodes);
      }
    }
  }
}

PointToPointTreeHelper::~PointToPointTreeHelper ()
{
}

Ptr<Node>
PointToPointTreeHelper::GetNode (uint32_t nLayer, uint32_t nGroup, uint32_t nNode)
{
  return NodeList::GetNode(GetNodeId(nLayer,nGroup,nNode));
}

Ipv4Address
PointToPointTreeHelper::GetIpv4Address (uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t nInterface)
{
  return GetNode(nLayer,nGroup,nNode)->GetObject<Ipv4>()->GetAddress(nInterface,0).GetLocal();
}

Ipv4Address
PointToPointTreeHelper::GetIpv4Address (uint32_t nodeId, uint32_t nInterface)
{
  return NodeList::GetNode(nodeId)->GetObject<Ipv4>()->GetAddress(nInterface,0).GetLocal();
}

void 
PointToPointTreeHelper::InstallStack (InternetStackHelper stack)
{
  stack.InstallAll();
}

void 
PointToPointTreeHelper::AssignIpv4Addresses (Ipv4AddressHelper address)
{
  std::ofstream ifs("myNodeList.csv");
  ifs<<"fromN fromA toN toA"<<std::endl;

  for(size_t nLayer=0; nLayer<(GetNLayers()-1); nLayer++){
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(m_bandwidths[nLayer]));
    p2p.SetChannelAttribute("Delay", StringValue(m_delays[nLayer]));
    for(size_t nGroup=0; nGroup<GetNGroups(nLayer); nGroup++){
      for(size_t nNode=0; nNode<GetNNodes(nLayer,nGroup); nNode++){
        for(int i=0; i<m_nodeNums[nLayer+1]; i++){
          Ptr<Node> fromNode = GetNode(nLayer, nGroup, nNode);
          Ptr<Node> toNode = GetNode(nLayer+1, nGroup*GetNNodes(nLayer,nGroup)+nNode, i);
          NetDeviceContainer device = p2p.Install(fromNode,toNode);
          Ipv4InterfaceContainer interface = address.Assign(device);
          ifs <<fromNode->GetId()<<" "<<InetSocketAddress(interface.GetAddress(0)).GetIpv4()<<" "
              <<toNode->GetId()<<" "<<InetSocketAddress(interface.GetAddress(1)).GetIpv4()<<std::endl;
          address.NewNetwork();
        }
      }
    }
  }
  ifs.close();
}

size_t PointToPointTreeHelper::GetNLayers(){
  return m_nodeNums.size();
}

size_t PointToPointTreeHelper::GetNGroups(uint32_t nLayer){
  uint32_t nodeNumInLayer = 1;
  for(uint32_t i=0; i<nLayer; i++){
    nodeNumInLayer *= m_nodeNums[i];
  }
  return nodeNumInLayer;
}

size_t PointToPointTreeHelper::GetNNodes(uint32_t nLayer, uint32_t nGroup){
  //in this time, each group has same number of nodes
  return m_nodeNums[nLayer];
}

uint32_t PointToPointTreeHelper::GetParentId(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t parentLayer){
  return GetParentId(GetNodeId(nLayer,nGroup,nNode),nLayer-parentLayer);
}

uint32_t PointToPointTreeHelper::GetParentId(uint32_t nodeId, uint32_t nUp){
  uint32_t id = nodeId;
  uint32_t preId = nodeId;
  for(uint32_t i=0; i<nUp; i++){
    preId = id;
    id = m_link[preId].GetParent();
  }
  return id;
}

Ipv4Address PointToPointTreeHelper::GetParentAddress(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t parentLayer){
  return GetParentAddress(GetNodeId(nLayer,nGroup,nNode),nLayer-parentLayer);
}

Ipv4Address PointToPointTreeHelper::GetParentAddress(uint32_t nodeId, uint32_t nUp){
  uint32_t id = nodeId;
  uint32_t preId = nodeId;
  for(uint32_t i=0; i<nUp; i++){
    preId = id;
    id = m_link[preId].GetParent();
  }
  if(id==0){
    return GetIpv4Address(id,m_link[id].GetNthChild(preId)+1);
  }
  return GetIpv4Address(id,m_link[id].GetNthChild(preId)+2);
}

std::vector<uint32_t> PointToPointTreeHelper::GetChildrenId(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t childLayer){
  return GetChildrenId(GetNodeId(nLayer,nGroup,nNode),childLayer-nLayer);
}

std::vector<uint32_t> PointToPointTreeHelper::GetChildrenId(uint32_t nodeId, uint32_t nDown){
  std::vector<uint32_t> childrenList;
  uint32_t id = nodeId;
  if(nDown==1){
    return m_link[id].GetChildren();
  }
  std::vector<uint32_t> children = m_link[id].GetChildren();
  for(uint32_t i: children){
    std::vector<uint32_t> tmpChildren = GetChildrenId(i,nDown-1);
    std::copy(tmpChildren.begin(),tmpChildren.end(),std::back_inserter(childrenList));
  }
  return childrenList;
}

std::vector<Ipv4Address> PointToPointTreeHelper::GetChildrenAddress(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t childLayer){
  return GetChildrenAddress(GetNodeId(nLayer,nGroup,nNode),childLayer-nLayer);
}

std::vector<Ipv4Address> PointToPointTreeHelper::GetChildrenAddress(uint32_t nodeId, uint32_t nDown){
  std::vector<uint32_t> childrenList = GetChildrenId(nodeId, nDown);
  std::vector<Ipv4Address> childrenAddr;
  for(uint32_t i: childrenList){
    childrenAddr.push_back(GetIpv4Address(i,1));
  }
  return childrenAddr;
}

std::vector<LinkContainer> PointToPointTreeHelper::GetLinkList(){
  return m_link;
}

uint32_t PointToPointTreeHelper::GetNodeId(uint32_t nLayer, uint32_t nGroup, uint32_t nNode){
  uint32_t nodeId = 0;
  if(nLayer==0){
    return nodeId;
  }
  uint32_t nodeNumInLayer = 1;
  for(uint32_t i=0; i<nLayer; i++){
    nodeNumInLayer *= m_nodeNums[i];
    nodeId += nodeNumInLayer;
  }
  nodeId += nGroup*m_nodeNums[nLayer];
  nodeId += nNode;
  return nodeId;
}

} // namespace ns3
