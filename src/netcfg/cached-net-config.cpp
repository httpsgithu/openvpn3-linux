//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2026-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2026-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   cached-net-config.cpp
 *
 * @brief  Helper class to prepare and cache a GVariant
 *         dictionary object holding the network configuration
 *         for a VPN session
 *
 *         The design used here is to store all the details for the main
 *         "sections" - remote, gateway, vpn_addresses and routes
 *         in temporary GVariant or GVariantBuilder objects through the
 *         phase where the AddRemote(), AddVPNaddress() and AddVPNroute()
 *         is being called.
 *
 *         Once the GetNetworkConfig() is called, all the temporary objects
 *         are gathered into the main configuration object (GVariant object)
 *         and the temporary objects are released/consumed.  This information
 *         is cached and calling the Add*() functions mentioned above will
 *         not have any effect any more.  To clear the cached configuration,
 *         this object need to be destroyed and a new object created.
 *
 *         Calling GetNetworkConfig() again after the cached configuration
 *         has been prepared, it will return the cached object each time.
 *
 *         NOTE: The caller of GetNetworkConfig() MUST NOT release the
 *         returned GVariant object.  If this object is passed on to
 *         a function consuming it, use g_variant_ref() on the returned
 *         object to increase the GVariant object's reference counter.
 */

#include "build-config.h"

#include <map>
#include <memory>
#include <string>
#include <gdbuspp/glib2/utils.hpp>

#include "log/core-dbus-logger.hpp"
#include <openvpn/netconf/linux/gwnetlink.hpp>
#include <openvpn/addr/ip.hpp>

#include "cached-net-config.hpp"
#include "netcfg-device.hpp"



namespace NetCfg {

CachedNetworkConfig::Ptr CachedNetworkConfig::Create()
{
    return CachedNetworkConfig::Ptr(new CachedNetworkConfig);
}


CachedNetworkConfig::~CachedNetworkConfig() noexcept
{
    if (cached_config)
    {
        g_variant_unref(cached_config);
    }
    if (remote)
    {
        g_variant_unref(remote);
    }
    if (gateways)
    {
        g_variant_unref(gateways);
    }
    if (bld_vpnips)
    {
        g_variant_builder_unref(bld_vpnips);
    }

    for (const auto &[table_name, table_data] : bld_routes)
    {
        if (table_data)
        {
            g_variant_builder_clear(bld_routes[table_name]);
            g_variant_builder_unref(bld_routes[table_name]);
        }
    }
}


void CachedNetworkConfig::AddRemote(const std::string &device_name, const std::string &remote_addr)
{
    if (remote)
    {
        g_variant_unref(remote);
    }
    remote = glib2::Value::Create(remote_addr);


    if (gateways)
    {
        g_variant_unref(gateways);
    }

    openvpn::LinuxGW46Netlink gw(device_name, remote_addr);
    GVariantDict *gw_dict = glib2::Dict::Create();
    if (gw.v4.defined())
    {
        glib2::Dict::Add(gw_dict, "IPv4", gw.v4.addr().to_string());
    }
    if (gw.v6.defined())
    {
        glib2::Dict::Add(gw_dict, "IPv6", gw.v4.addr().to_string());
    }
    gateways = glib2::Dict::Finish(gw_dict);
}


std::map<std::string, std::string> CachedNetworkConfig::VPNaddressDetails(const VPNAddress &address)
{
    return {{"ip_version", (address.ipv6 ? "6" : "4")},
            {"ip_address", address.address},
            {"prefix_size", std::to_string(address.prefix_size)}};
}


std::map<std::string, std::string> CachedNetworkConfig::AddVPNaddress(const VPNAddress &address, const std::string &gw_ipv4, const std::string &gw_ipv6)
{
    if (!bld_vpnips)
    {
        bld_vpnips = glib2::Builder::Create("a(sus)");
    }

    GVariantBuilder *ip_entry = glib2::Builder::Create("(sus)");
    glib2::Builder::Add<std::string>(ip_entry, address.address);
    glib2::Builder::Add<uint32_t>(ip_entry, address.prefix_size);
    glib2::Builder::Add<std::string>(ip_entry, (address.ipv6 ? gw_ipv6 : gw_ipv4));
    glib2::Builder::Add(bld_vpnips, glib2::Builder::Finish(ip_entry));

    return VPNaddressDetails(address);
}


std::map<std::string, std::string> CachedNetworkConfig::VPNNetworkDetails(const Network &network, const std::string &gw_ipv4, const std::string &gw_ipv6)
{
    std::map<std::string, std::string> ret = {{"ip_version", (network.ipv6 ? "6" : "4")},
                                              {"subnet", network.address},
                                              {"prefix_size", std::to_string(network.prefix_size)}};
    if ((network.ipv6 && !gw_ipv6.empty()) || (!network.ipv6 && !gw_ipv4.empty()))
    {
        ret["gateway"] = (network.ipv6 ? gw_ipv6 : gw_ipv4);
    }

    return ret;
}


std::map<std::string, std::string> CachedNetworkConfig::AddVPNroute(const Network &network, const std::string &gw_ipv4, const std::string &gw_ipv6)
{
    RtTable rt_tbl;
    if (network.ipv6)
    {
        rt_tbl = (network.exclude ? RtTable::IPV6_EXCLUDE : RtTable::IPV6_INCLUDE);
    }
    else
    {
        rt_tbl = (network.exclude ? RtTable::IPV4_EXCLUDE : RtTable::IPV4_INCLUDE);
    }

    if (bld_routes.find(rt_tbl) == bld_routes.end())
    {
        bld_routes[rt_tbl] = glib2::Builder::Create("a(sun)");
    }

    GVariantBuilder *rt_entry = glib2::Builder::Create("(sun)");
    glib2::Builder::Add<std::string>(rt_entry, network.address);
    glib2::Builder::Add<uint32_t>(rt_entry, network.prefix_size);
    glib2::Builder::Add<int16_t>(rt_entry, network.metric);
    glib2::Builder::Add(bld_routes[rt_tbl], glib2::Builder::Finish(rt_entry));

    return VPNNetworkDetails(network, gw_ipv4, gw_ipv6);
}


GVariant *CachedNetworkConfig::GetNetworkConfig()
{
    // If there is no cached configuration, populate it
    if (!cached_config)
    {
        // Create the main configuration dictionary
        GVariantBuilder *config = glib2::Builder::Create("a{sv}");
        glib2::Builder::AddKeyValue<std::string, GVariant *>(config, "remote", remote);
        glib2::Builder::AddKeyValue<std::string, GVariant *>(config, "gateway", gateways);
        glib2::Builder::AddKeyValue<std::string, GVariant *>(config, "vpn_addresses", glib2::Builder::Finish(bld_vpnips));

        // Prepare the routes sub-dictionary
        GVariantBuilder *routes = glib2::Builder::Create("a{sv}");

        // Helper lambda to only add the routing tables being used
        auto add_route_entries = [&](GVariantBuilder *bld, const std::string &section, RtTable table)
        {
            // If the the given routing table has some entries, add it
            if (bld_routes.find(table) != bld_routes.end())
            {
                glib2::Builder::AddKeyValue<std::string, GVariant *>(bld, section, glib2::Builder::Finish(bld_routes[table]));
            }
        };

        // Prepare the `include` routing sub-dictionary, with the IPv4 and IPv6 sub-sections
        GVariantBuilder *include_routes = glib2::Builder::Create("a{sv}");
        add_route_entries(include_routes, "IPv4", RtTable::IPV4_INCLUDE);
        add_route_entries(include_routes, "IPv6", RtTable::IPV6_INCLUDE);
        glib2::Builder::AddKeyValue<std::string, GVariant *>(routes, "include", glib2::Builder::Finish(include_routes));

        // Prepare the `exclude` routing sub-dictionary, with the IPv4 and IPv6 sub-sections
        GVariantBuilder *exclude_routes = glib2::Builder::Create("a{sv}");
        add_route_entries(exclude_routes, "IPv4", RtTable::IPV4_EXCLUDE);
        add_route_entries(exclude_routes, "IPv6", RtTable::IPV6_EXCLUDE);
        glib2::Builder::AddKeyValue<std::string, GVariant *>(routes, "exclude", glib2::Builder::Finish(exclude_routes));

        // Add the `routes` section to the main configuration dictionary
        glib2::Builder::AddKeyValue<std::string, GVariant *>(config, "routes", glib2::Builder::Finish(routes));

        // Cache the configuration
        cached_config = glib2::Builder::Finish(config);

        // Clean up the elements used creating the configuration dictionary.
        // These are consumed by the glib2::Builder::AddKeyValue() or glib2::Builder::Finish(),
        // so "mark" them cleared by setting them to nullptr.
        remote = nullptr;
        gateways = nullptr;
        bld_vpnips = nullptr;
        bld_routes.clear();
    }

    return cached_config;
}

} // namespace NetCfg