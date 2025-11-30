//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2018 - 2023  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2018 - 2023  David Sommerseth <davids@openvpn.net>
//  Copyright (C) 2019 - 2023  Lev Stipakov <lev@openvpn.net>
//

#include "netcfg-changeevent.hpp"


NetCfgChangeEvent::NetCfgChangeEvent(const NetCfgChangeType &t,
                                     const std::string &dev,
                                     const NetCfgChangeDetails &d) noexcept
{
    reset();
    type = t;
    device = dev;
    details = d;
}


NetCfgChangeEvent::NetCfgChangeEvent(GVariant *params)
{
    std::string g_type = glib2::DataType::Extract(params);
    if ("(usa{ss})" != g_type)
    {
        throw NetCfgException(std::string("Invalid GVariant data type: ")
                              + g_type);
    }

    type = glib2::Value::Extract<NetCfgChangeType>(params, 0);
    device = glib2::Value::Extract<std::string>(params, 1);
    GVariant *details_dict = glib2::Value::ExtractChild(params, 2);

    details.clear();
    auto parse_details = [this](GVariant *record)
    {
        auto key = glib2::Value::Extract<std::string>(record, 0);
        auto value = glib2::Value::Extract<std::string>(record, 1);
        details.insert({key, value});
    };
    glib2::Value::IterateArray(details_dict, parse_details);
}


NetCfgChangeEvent::NetCfgChangeEvent() noexcept
{
    reset();
}


void NetCfgChangeEvent::reset() noexcept
{
    type = NetCfgChangeType::UNSET;
    device.clear();
    details.clear();
}


bool NetCfgChangeEvent::empty() const noexcept
{
    return (NetCfgChangeType::UNSET == type
            && device.empty() && details.empty());
}


GVariant *NetCfgChangeEvent::GetGVariant() const
{
    GVariantBuilder *b = glib2::Builder::Create("(usa{ss})");
    glib2::Builder::Add(b, type);
    glib2::Builder::Add(b, device);

    glib2::Builder::OpenChild(b, "a{ss}");
    for (const auto &[key, value] : details)
    {
        // WARNING: For some odd reason, these four lines
        // below this code context triggers a memory leak
        // warning in valgrind.  This is currently believed
        // to be a false positive, as extracting this code
        // to a separate and minimal program does not trigger
        // this leak warning despite being practically the same
        // code.
        glib2::Builder::AddKeyValue<std::string, std::string>(b, key, value);
    }
    glib2::Builder::CloseChild(b);
    return glib2::Builder::Finish(b);
}


bool NetCfgChangeEvent::operator==(const NetCfgChangeEvent &compare) const
{
    return ((compare.type == (const NetCfgChangeType)type)
            && (0 == compare.device.compare(device))
            && (compare.details == details));
}


bool NetCfgChangeEvent::operator!=(const NetCfgChangeEvent &compare) const
{
    return !(this->operator==(compare));
}
