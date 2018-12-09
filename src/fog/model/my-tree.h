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

#ifndef MY_TREE_HELPER_H
#define MY_TREE_HELPER_H

#include <string>
#include <vector>

#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/applications-module.h"

namespace ns3 {

class LinkContainer{
public:
  LinkContainer();
  LinkContainer(uint32_t parent);
  ~LinkContainer();
  void SetParent(uint32_t parent);
  void SetChildren(std::vector<uint32_t> children);
  void AddChild(uint32_t child);
  uint32_t GetParent();
  uint32_t GetNthChild(uint32_t child);
  size_t GetNChildren();
  uint32_t GetChild(uint32_t nth);
  std::vector<uint32_t> GetChildren();
  bool IsChildEmpty();
  static LinkContainer CreateLinkContainer();
  static LinkContainer CreateLinkContainer(uint32_t parent);

private:
  uint32_t m_parent;
  std::vector<uint32_t> m_children;
};

class PointToPointTreeHelper
{
public:
  PointToPointTreeHelper (std::vector<int> nodeNums, std::vector<std::string> bandwidth, std::vector<std::string> delays);
  PointToPointTreeHelper();

  ~PointToPointTreeHelper ();

public:
  Ptr<Node> GetNode (uint32_t nLayer, uint32_t nGroup, uint32_t nNode);

  Ipv4Address GetIpv4Address (uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t nInterface);
  Ipv4Address GetIpv4Address (uint32_t nodeId, uint32_t nInterface);

  void InstallStack (InternetStackHelper stack);

  void AssignIpv4Addresses (Ipv4AddressHelper address);

  size_t GetNLayers();
  size_t GetNGroups(uint32_t nLayer);
  size_t GetNNodes(uint32_t nLayer, uint32_t nGroup);

  uint32_t GetParentId(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t parentLayer);
  uint32_t GetParentId(uint32_t nodeId, uint32_t nUp);
  Ipv4Address GetParentAddress(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t parentLayer);
  Ipv4Address GetParentAddress(uint32_t nodeId, uint32_t nUp);

  std::vector<uint32_t> GetChildrenId(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t childLayer);
  std::vector<uint32_t> GetChildrenId(uint32_t nodeId, uint32_t nDown);
  std::vector<Ipv4Address> GetChildrenAddress(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t childLayer);
  std::vector<Ipv4Address> GetChildrenAddress(uint32_t nodeId, uint32_t nDown);

  std::vector<LinkContainer> GetLinkList();

  uint32_t GetNodeId(uint32_t nLayer, uint32_t nGroup, uint32_t nNode);

  template < class T > ApplicationContainer InstallApp(T& app, uint32_t nLayer);
  template < class T > ApplicationContainer InstallApp(T& app, uint32_t nLayer, uint32_t nGroup, uint32_t nNode);

private:
  std::vector<int> m_nodeNums;
  std::vector<std::string> m_bandwidths;
  std::vector<std::string> m_delays;
  std::vector<LinkContainer> m_link;
};

template < class T > ApplicationContainer PointToPointTreeHelper::InstallApp(T& app, uint32_t nLayer){
  ApplicationContainer appCon;
  for(size_t i=0;i<GetNGroups(nLayer);i++){
    for(size_t j=0;j<GetNNodes(nLayer,i);j++){
      appCon.Add(app.Install(GetNode(nLayer,i,j)));
    }
  }
  return appCon;
}

template < class T > ApplicationContainer PointToPointTreeHelper::InstallApp(T& app, uint32_t nLayer, uint32_t nGroup, uint32_t nNode){
  ApplicationContainer appCon;
  appCon.Add(app.Install(GetNode(nLayer,nGroup,nNode)));
  return appCon;
}

} // namespace ns3

#endif /* POINT_TO_POINT_STAR_HELPER_H */
