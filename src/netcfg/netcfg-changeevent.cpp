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
    std::string g_type(g_variant_get_type_string(params));
    if ("(usa{ss})" != g_type)
    {
        throw NetCfgException(std::string("Invalid GVariant data type: ")
                              + g_type);
    }

    gchar *dev = nullptr;
    GVariantIter *det = nullptr;
    guint tmp_type = 0;
    g_variant_get(params, "(usa{ss})", &tmp_type, &dev, &det);
    type = (NetCfgChangeType)tmp_type;

    device = std::string(dev);
    g_free(dev);
    details.clear();

    GVariant *kv = nullptr;
    while ((kv = g_variant_iter_next_value(det)))
    {
        gchar *key = nullptr;
        gchar *val = nullptr;
        g_variant_get(kv, "{ss}", &key, &val);
        details[key] = std::string(val);
        g_free(key);
        g_free(val);
    }
    g_variant_iter_free(det);
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
