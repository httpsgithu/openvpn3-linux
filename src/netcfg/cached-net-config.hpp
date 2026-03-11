//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2026-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2026-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   cached-net-config.hpp
 *
 * @brief  Helper class to prepare and cache a GVariant
 *         dictionary object holding the network configuration
 *         for a VPN session
 */

#pragma once

#include <map>
#include <memory>
#include <string>

#include "netcfg-device.hpp"


namespace NetCfg {

/**
 *  Helper class to cache a GVariant object containting the
 *  current network configuration
 *
 *  The content this object has prepared is populating
 *  the net.openvpn.v3.netcfg.network_config property
 *  in the NetCfgDevice class.
 *
 */
class CachedNetworkConfig
{
  public:
    using Ptr = std::shared_ptr<CachedNetworkConfig>;

    /**
     *  Create a "human readable map" for the four
     *  routing tables a VPN configuration can have
     */
    enum class RtTable : uint8_t const
    {
        IPV4_INCLUDE = 0, //< Ordinary IPv4 routes, routed via the VPN tunnel
        IPV4_EXCLUDE,     //< IPv4 routes excluded from the tunnel
        IPV6_INCLUDE,     //< Ordinary IPv6 routes, routed via the VPN tunnel
        IPV6_EXCLUDE,     //< IPv6 routes excluded from the tunnel
    };

    [[nodiscard]] static CachedNetworkConfig::Ptr Create();
    ~CachedNetworkConfig() noexcept;

    /**
     *  Store the current remote host this VPN session is connected to,
     *  together with the gateway used to access this remote host.
     *
     * @param device_name   Device name to be excluded from finding the gateway.
     *                      This is needed to avoid catching a route via the
     *                      VPN tunnel, which is not the route used to access this
     *                      host
     * @param remote_addr   Remote host this session is connected to
     */
    void AddRemote(const std::string &device_name, const std::string &remote_addr);

    /**
     *  Helper function to generate the dictionary used for sending NetworChanged notification signals.
     *
     *  Since this class need to do some parsing of the VPNAddress object data, it can generate this
     *  information on-the-fly while at it.
     *
     * @param address    VPNAddress object to parse
     * @return std::map<std::string, std::string> Returns a key/value dictionary with information for the
     *         D-Bus signal to be sent.
     */
    [[nodiscard]] static std::map<std::string, std::string> VPNaddressDetails(const VPNAddress &address);

    /**
     *  Cache a VPN IP address this VPN session is configured to use
     *
     * @param address   VPNaddress object with the VPN address
     * @param gw_ipv4   IPv4 gateway assigned to this VPN address, if it is an IPv4 address
     * @param gw_ipv6   IPv6 gateway assigned to this VPN address, if it is an IPv6 address
     * @return std::map<std::string, std::string> This returns the result of VPNAddressDetails() which is given
     *         the same address object this method has received.  Used for the NetworkChanged D-Bus signal.
     */
    std::map<std::string, std::string> AddVPNaddress(const VPNAddress &address, const std::string &gw_ipv4, const std::string &gw_ipv6);

    /**
     *  Helper function to generate the dictionary used for sending NetworkChanged notification signals.
     *
     *  Since this class need to do some parsing of the Network object data, it can generate this
     *  information on-the-fly while at it.  While VPNaddressDetails() processes VPNAddress objects,
     *  this method processes Network objects
     *
     * @param network  Network object containing the IP route to process
     * @param gw_ipv4  (optional) If the Network object refers to an IPv4 address, this is the IPv4 gateway being used
     * @param gw_ipv6  (optional) If the Network object refers to an IPv6 address, this is the IPv6 gateway being used
     * @return std::map<std::string, std::string>  Returns a key/value dictionary with information for the
     *         D-Bus signal to be sent.
     */
    [[nodiscard]] static std::map<std::string, std::string> VPNNetworkDetails(const Network &network, const std::string &gw_ipv4 = "", const std::string &gw_ipv6 = "");

    /**
     *  Cache a VPN network route this VPN session uses
     *
     * @param network   Network object containing the route details
     * @param gw_ipv4   The IPv4 gateway for the network route, if the network is an IPv4 subnet
     * @param gw_ipv6   The IPv6 gateway for the network route, if the network is an IPv6 subnet
     * @return std::map<std::string, std::string>  This returns the result of VPNNetworkDetails() which is given
     *         the same network object this method has received.  Used for the NetworkChanged D-Bus signal.
     */
    std::map<std::string, std::string> AddVPNroute(const Network &network, const std::string &gw_ipv4, const std::string &gw_ipv6);

    /**
     *  Retrieve the GVariant based key/value dictionary containing the VPN network configuration
     *
     *  This will create the dictionary as a GVariant object on the first access and preserve it
     *  if this function is called again later for quick access to the information.
     *
     *  The returned value MUST NOT be released by g_variant_unref().  If the consumer of the
     *  returned GVariant object will result in the reference counter being reduced, it must
     *  make use of the g_variant_ref() function on the object returned by this function.
     *
     * @return GVariant*
     */
    GVariant *GetNetworkConfig();

  private:
    GVariantBuilder *bld_vpnips = nullptr;
    std::unordered_map<RtTable, GVariantBuilder *> bld_routes;
    GVariant *remote = nullptr;
    GVariant *gateways = nullptr;
    GVariant *cached_config = nullptr;
    CachedNetworkConfig() = default;
};

} // namespace NetCfg