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

namespace ns3 {

/**
 * \defgroup point-to-point-layout Point-to-Point Layout Helpers
 *
 */

/**
 * \ingroup point-to-point-layout
 *
 * \brief A helper to make it easier to create a star topology
 * with PointToPoint links
 */
class PointToPointTreeHelper
{
public:
  /**
   * Create a PointToPointTreeHelper in order to easily create
   * star topologies using p2p links
   *
   * \param numSpokes the number of links attached to 
   *        the hub node, creating a total of 
   *        numSpokes + 1 nodes
   *
   * \param p2pHelper the link helper for p2p links, 
   *        used to link nodes together
   */
  PointToPointTreeHelper (std::vector<int> nodeNums, std::vector<std::string> bandwidth, std::vector<std::string> delays);

  ~PointToPointTreeHelper ();

public:
  Ptr<Node> GetNode (uint32_t nLayer, uint32_t nGroup, uint32_t nNode) const;

  Ipv4Address GetIpv4Address (uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t nInterface) const;

  void InstallStack (InternetStackHelper stack);

  void AssignIpv4Addresses (Ipv4AddressHelper address);

  size_t GetNLayers();

  size_t GetNGroups(uint32_t nLayer);

  size_t GetNNodes(uint32_t nLayer, uint32_t nGroup);

  Ipv4Address GetParentAddress(uint32_t nLayer, uint32_t nGroup, uint32_t nNode, uint32_t parentLayer);

private:
  std::vector<std::vector<NodeContainer>> m_layers;
  std::vector<NetDeviceContainer> m_devices;
  std::vector<Ipv4InterfaceContainer> m_interfaces;
  std::vector<int> m_nodeNums;
  std::vector<std::string> m_bandwidths;
  std::vector<std::string> m_delays;
};

} // namespace ns3

#endif /* POINT_TO_POINT_STAR_HELPER_H */
