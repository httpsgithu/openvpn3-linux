//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2020-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2020-  David Sommerseth <davids@openvpn.net>
//

/**
 * @file   proxy-systemd-resolved.cpp
 *
 * @brief  D-Bus proxy for the systemd-resolved service
 */

#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>
#include <asio.hpp>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/format-inl.h>

#include <gdbuspp/connection.hpp>
#include <gdbuspp/glib2/utils.hpp>
#include <gdbuspp/object/path.hpp>
#include <gdbuspp/proxy.hpp>
#include <gdbuspp/proxy/utils.hpp>

#include <openvpn/common/split.hpp>

#include "log/core-dbus-logger.hpp"
#include "dbus/support-functions.hpp"
#include "netcfg/dns/proxy-systemd-resolved.hpp"
#include "netcfg/dns/systemd-resolved-exception.hpp"

using namespace NetCfg::DNS::resolved;

/**
 *  Enable low-level debug logging.  This is not expected to
 *  be needed to be enabled in production environments, thus
 *  it is hard-coded in here.
 */
// #define DEBUG_RESOLVED_DBUS

namespace {
/**
 *  Low-level debug logging for background D-Bus calls to
 *  systemd-resolved.
 *
 *  This systemd-resolved proxy code does not have direct
 *  access to the logging infrastructure used by other parts
 *  of the NetCfg service.  Instead, we make use of the primitive
 *  debug logging in the OpenVPN 3 Core library, with a little
 *  adjustment to differentiate these log events from the Core
 *  library.
 *
 */

template <typename... T>
using vargs =
    fmt::detail::format_arg_store<fmt::context, sizeof...(T), fmt::detail::count_named_args<T...>(), fmt::detail::make_descriptor<fmt::context, T...>()>;

template <typename... T>
void sd_resolved_bg_log(fmt::format_string<T...> fmt, T &&...args)
{
    std::string msg = fmt::vformat(fmt.str, vargs<T...>{{args...}});
    CoreLog::___core_log("systemd-resolved background proxy", std::move(msg));
}


#ifdef DEBUG_RESOLVED_DBUS
template <typename... T>
void sd_resolved_debug(fmt::format_string<T...> fmt, T &&...args)
{
    std::string msg = fmt::vformat(fmt.str, vargs<T...>{{args...}});
    CoreLog::___core_log(" <DEBUG>   systemd-resolved background proxy", std::move(msg));
}

#else
#define sd_resolved_debug(...)
#endif

} // namespace

namespace NetCfg {
namespace DNS {
namespace resolved {

namespace Error {

std::mutex access_mtx;

Message::Message(const std::string &method_,
                 const std::string &message_)
    : method(method_), message(message_)
{
}


std::string Message::str() const
{
    return "[" + method + "] " + message;
}

Storage::Ptr Storage::Create()
{
    return Storage::Ptr(new Storage());
}

Storage::Storage() = default;
Storage::~Storage() noexcept = default;


void Storage::Add(const std::string &link, const std::string &method, const std::string &message)
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    errors_[link].emplace_back(method, message);
}


std::vector<std::string> Storage::GetLinks() const
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    std::vector<std::string> ret;
    for (const auto &[link, err] : errors_)
    {
        ret.push_back(link);
    }
    return ret;
}


size_t Storage::NumErrors(const std::string &link) const
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    auto f = errors_.find(link);
    return (f != errors_.end() ? f->second.size() : 0);
}


Error::Message::List Storage::GetErrors(const std::string &link)
{
    std::lock_guard<std::mutex> lock_guard(Error::access_mtx);
    auto recs = errors_.extract(link);
    return !recs.empty() ? recs.mapped() : Error::Message::List{};
}

} // namespace Error



//
//  NetCfg::DNS::resolved::SearchDomain
//
SearchDomain::SearchDomain(const std::string &srch, const bool rout)
    : search(srch), routing(rout)
{
}


SearchDomain::SearchDomain(GVariant *entry)
{
    glib2::Utils::checkParams(__func__, entry, "(sb)", 2);

    search = glib2::Value::Extract<std::string>(entry, 0);
    routing = glib2::Value::Extract<bool>(entry, 1);
}


GVariant *SearchDomain::GetGVariant() const
{
    if (search.empty())
    {
        return nullptr;
    }
    GVariantBuilder *b = glib2::Builder::Create("(sb)");
    glib2::Builder::Add(b, search);
    glib2::Builder::Add(b, routing);
    return glib2::Builder::Finish(b);
}



//
//  NetCfg::DNS::resolved::Link
//

Link::Ptr Link::Create(asio::io_context &asio_ctx,
                       Error::Storage::Ptr errors,
                       DBus::Proxy::Client::Ptr prx,
                       int32_t if_index,
                       const DBus::Object::Path &path,
                       const std::string &devname)
{
    return Link::Ptr(new Link(asio_ctx, std::move(errors), std::move(prx), if_index, path, devname));
}


Link::Link(asio::io_context &asio_ctx,
           Error::Storage::Ptr errors_,
           DBus::Proxy::Client::Ptr prx,
           int32_t if_idx,
           const DBus::Object::Path &path,
           const std::string &devname)
    : asio_proxy(asio_ctx), errors(std::move(errors_)), proxy(std::move(prx)),
      if_index(if_idx), device_name(devname)
{
    tgt_link = DBus::Proxy::TargetPreset::Create(path,
                                                 "org.freedesktop.resolve1.Link");
    tgt_mgmt = DBus::Proxy::TargetPreset::Create("/org/freedesktop/resolve1",
                                                 "org.freedesktop.resolve1.Manager");
}


DBus::Object::Path Link::GetPath() const
{
    return (tgt_link ? tgt_link->object_path : "");
}


std::string Link::GetDeviceName() const
{
    return device_name;
}


std::vector<std::string> Link::GetDNSServers() const
{
    GVariant *r = proxy->GetPropertyGVariant(tgt_link, "DNS");
    glib2::Utils::checkParams(__func__, r, "a(iay)");

    std::vector<std::string> dns_srvs;
    auto parse_record = [&dns_srvs](GVariant *rec)
    {
        IPAddress d(rec);
        dns_srvs.push_back(d.str());
    };
    glib2::Value::IterateArray(r, parse_record);
    g_variant_unref(r);

    return dns_srvs;
}


std::vector<std::string> Link::SetDNSServers(const IPAddress::List &servers)
{
    GVariantBuilder *b = glib2::Builder::Create("(ia(iay))");
    glib2::Builder::Add(b, if_index);

    glib2::Builder::OpenChild(b, "a(iay)");
    std::vector<std::string> applied{};
    for (const auto &srv : servers)
    {
        glib2::Builder::Add(b, srv.GetGVariant());
        applied.push_back(srv.str());
    }
    glib2::Builder::CloseChild(b);

    BackgroundCall(tgt_mgmt, "SetLinkDNS", glib2::Builder::Finish(b));
    return applied;
}


std::string Link::GetCurrentDNSServer() const
{
    GVariant *r = nullptr;
    try
    {
        r = proxy->GetPropertyGVariant(tgt_link, "CurrentDNSServer");
        IPAddress d(r);
        g_variant_unref(r);
        return d.str();
    }
    catch (const Exception &)
    {
        // Ignore exceptions and instead return an empty server
        // in this case
        g_variant_unref(r);
        return "";
    }
    catch (const DBus::Exception &)
    {
        return "";
    }
}


SearchDomain::List Link::GetDomains() const
{
    GVariant *r = proxy->GetPropertyGVariant(tgt_link, "Domains");
    glib2::Utils::checkParams(__func__, r, "a(sb)");

    SearchDomain::List ret{};
    auto parse_record = [&ret](GVariant *record)
    {
        SearchDomain dom(record);
        ret.push_back(dom);
    };
    glib2::Value::IterateArray(r, parse_record);
    g_variant_unref(r);

    return ret;
}


std::vector<std::string> Link::SetDomains(const SearchDomain::List &doms)
{
    GVariantBuilder *b = glib2::Builder::Create("(ia(sb))");
    glib2::Builder::Add(b, if_index);

    glib2::Builder::OpenChild(b, "a(sb)");
    std::vector<std::string> applied{};
    for (const auto &dom : doms)
    {
        GVariant *r = dom.GetGVariant();
        if (r)
        {
            glib2::Builder::Add(b, r);
            applied.push_back(dom.search);
        }
    }
    glib2::Builder::CloseChild(b);
    BackgroundCall(tgt_mgmt, "SetLinkDomains", glib2::Builder::Finish(b));
    return applied;
}


bool Link::GetDefaultRoute() const
{
    try
    {
        return proxy->GetProperty<bool>(tgt_link, "DefaultRoute");
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception("Could not extract DefaultRoute");
    }
}


void Link::SetDefaultRoute(const bool route)
{
    if (!feature_set_default_route)
    {
        return;
    }

    GVariantBuilder *b = glib2::Builder::Create("(ib)");
    glib2::Builder::Add(b, if_index);
    glib2::Builder::Add(b, route);

    BackgroundCall(tgt_mgmt,
                   "SetLinkDefaultRoute",
                   glib2::Builder::Finish(b),
                   [self = shared_from_this()](const std::vector<std::string> errormsgs)
                   {
                       for (const auto &err : errormsgs)
                       {
                           self->errors->Add(self->tgt_link->object_path, "SetLinkDefaultRoute", err);
                       }
                       self->feature_set_default_route = false;
                   });
}


bool Link::GetFeatureSetDefaultRoute() const
{
    return feature_set_default_route;
}


std::string Link::GetDNSSEC() const
{
    try
    {
        return proxy->GetProperty<std::string>(tgt_link, "DNSSEC");
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception("Could not retrieve DNSSEC mode: "
                        + std::string(excp.GetRawError()));
    }
}


void Link::SetDNSSEC(const std::string &mode)
{
    if (mode != "yes" && mode != "no" && mode != "allow-downgrade")
    {
        throw Exception("Invalid DNSSEC mode requested: " + mode);
    }

    GVariantBuilder *b = glib2::Builder::Create("(is)");
    glib2::Builder::Add(b, if_index);
    glib2::Builder::Add(b, mode);
    BackgroundCall(tgt_mgmt, "SetLinkDNSSEC", glib2::Builder::Finish(b));
}


std::string Link::GetDNSOverTLS() const
{
    try
    {
        return proxy->GetProperty<std::string>(tgt_link, "DNSOverTLS");
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception("Could not retrieve DNSOverTLS mode: "
                        + std::string(excp.GetRawError()));
    }
}


void Link::SetDNSOverTLS(const std::string &mode)
{
    if (mode != "no" && mode != "false"
        && mode != "yes" && mode != "true"
        && mode != "opportunistic")
    {
        throw Exception("Invalid DNSOverTLS mode requested: " + mode);
    }

    GVariantBuilder *b = glib2::Builder::Create("(is)");
    glib2::Builder::Add(b, if_index);
    glib2::Builder::Add(b, mode);
    BackgroundCall(tgt_mgmt, "SetLinkDNSOverTLS", glib2::Builder::Finish(b));
}


void Link::Revert()
{
    BackgroundCall(tgt_mgmt, "RevertLink", glib2::Value::Create(if_index));
}


Error::Message::List Link::GetErrors() const
{
    return errors->GetErrors(tgt_link->object_path);
}



void Link::WaitForBackgroundTasks() const
{
    // TODO: This is very primitive.  But good enough for now.
    //       This method is primarily used by the test programs currently
    //       In the future, std::atomic::wait() in C++20
    //       https://en.cppreference.com/w/cpp/atomic/atomic/wait.html
    while (asio_running_tasks > 0)
    {
        sleep(1);
    }
}

namespace {
/**
 *  Simple hack to simplify passing data from BackgroundCall()
 *  to the lambda function performing the operation
 */
struct background_call_data
{
    using Ptr = std::shared_ptr<background_call_data>;
    background_call_data(DBus::Proxy::Client::Ptr prx,
                         const DBus::Object::Path &objpath,
                         const std::string &interf,
                         const std::string &meth,
                         GVariant *prms,
                         std::function<void(const std::vector<std::string> &)> err_cb)
        : proxy(std::move(prx)), object_path(objpath), interface(interf),
          method(meth), params(prms), error_callback(std::move(err_cb))
    {
    }

    DBus::Proxy::Client::Ptr proxy;
    DBus::Object::Path object_path;
    std::string interface;
    std::string method;
    GVariant *params;
    std::function<void(const std::vector<std::string> &errormsg)> error_callback;
};
} // namespace


void Link::BackgroundCall(DBus::Proxy::TargetPreset::Ptr &target,
                          const std::string &method,
                          GVariant *params,
                          std::function<void(const std::vector<std::string> &errormsg)> error_callback)
{
    if (asio_proxy.stopped())
    {
        sd_resolved_debug("Background ASIO thread not running");
        throw Exception("Background ASIO thread not running");
    }

    sd_resolved_debug("Preparing ASIO post lambda: error-object={} proxy={} target={} interface={} method={} params='{}'",
                      (errors ? "[valid]" : "[invalid]"),
                      (proxy ? proxy->GetDestination() : "[invalid proxy object]"),
                      (target ? target->object_path : "[invalid target object]"),
                      (target ? target->interface : "[invalid target object]"),
                      method,
                      glib2::Utils::DumpToString(params));

    if (!target)
    {
        throw Exception("systemd-resolved network interface target undefined (tgt_link)");
    }

    if (asio_running_tasks + 1 >= UINT16_MAX)
    {
        throw Exception("Too many bacground ASIO tasks running");
    }
    asio_running_tasks++;

    /*
     *  // TODO: Improve this
     *
     *  This is an ugly hack.  For some odd reason, the tgt_link object
     *  is often ending up invalid inside the lambda function, resulting
     *  in failing updates sent to the systemd-resolved service because the
     *  object path in the DBus::Proxy::TargetPreset object ends up invalid.
     *  This happens despite the systemd-resolved D-Bus object existing and
     *  can be configured.
     *
     *  The proxy object seems to be handled fine, so we keep the "old"
     *  approach here.
     */
    background_call_data::Ptr bgdata = std::make_shared<background_call_data>(
        proxy,
        target->object_path,
        target->interface,
        method,
        (params ? g_variant_ref(params) : nullptr),
        error_callback);


    if (nullptr == bgdata)
    {
        throw Exception(fmt::format(
            FMT_COMPILE("Failed to allocate memory buffer for background task: path={}, interface={}, method={}, params='{}'"),
            target->object_path,
            target->interface,
            method,
            glib2::Utils::DumpToString(params)));
    }

    asio::post(
        asio_proxy,
        [bgdata, running_tasks_count = &asio_running_tasks]()
        {
            std::vector<std::string> error_messages;
            try
            {
                DBus::Proxy::Client::Ptr proxy = std::move(bgdata->proxy);
                if (!proxy)
                {
                    sd_resolved_bg_log("Invalid background request: proxy={} object_path={} method={}.{} params='{}",
                                       (proxy ? proxy->GetDestination() : "[invalid]"),
                                       bgdata->object_path,
                                       bgdata->interface,
                                       bgdata->method,
                                       glib2::Utils::DumpToString(bgdata->params));

                    //  If the proxy object is invalid, the Link object has been
                    //  or is being destructed.  Then we just bail out.
                    if (bgdata->params)
                    {
                        g_variant_unref(bgdata->params);
                    }
                    return;
                }

                // It might be the call to systemd-resolved times out,
                // so we're being a bit more persistent in these background
                // calls
                auto prxqry = DBus::Proxy::Utils::Query::Create(proxy);
                for (uint8_t attempts = 3; attempts > 0; attempts--)
                {
                    try
                    {
                        if (!GDBusPP::Proxy::Utils::LookupObject(proxy, bgdata->object_path))
                        {
                            sd_resolved_bg_log("[LAMBDA] target={}, interface={}, method={}, attempts={} - Object not found",
                                               bgdata->object_path,
                                               bgdata->interface,
                                               bgdata->method,
                                               attempts);
                            sleep(1);
                            continue; // Retry again
                        }

                        sd_resolved_debug("[LAMBDA] Performing proxy call: object_path={}, method={}.{}, params='{}'",
                                          bgdata->path,
                                          bgdata->interface,
                                          bgdata->method,
                                          glib2::Utils::DumpToString(bgdata->params));

                        // The proxy->Call(...) call might result in bgdata->params being released,
                        // even if an exception happens.  We increase the GVariant refcounter to
                        // avoid this object being deleted just yet.
                        GVariant *params = (bgdata->params ? g_variant_ref_sink(bgdata->params) : nullptr);
                        GVariant *r = proxy->Call(bgdata->object_path, bgdata->interface, bgdata->method, params);
                        g_variant_unref(r);
                        error_messages = {};
                        break;
                    }
                    catch (const std::exception &excp)
                    {
                        std::string err = excp.what();
                        error_messages.push_back(err);
                        sd_resolved_debug("[LAMBDA]  proxy call exception, object_path={}: {}",
                                          bgdata->path,
                                          err);
                        if ((err.find("Timeout was reached") != std::string::npos) || attempts < 1)
                        {
                            sd_resolved_bg_log("Background systemd-resolved call failed: object_path={}, method={}.{}: {}",
                                               bgdata->object_path,
                                               bgdata->interface,
                                               bgdata->method,
                                               err);
                        }
                        sleep(1);
                    }
                }

                if (bgdata->error_callback && error_messages.size() > 0)
                {
                    bgdata->error_callback(error_messages);
                }

                // Delete the GVariant object with the D-Bus method arguments; now it is no longer needed
                if (bgdata->params)
                {
                    g_variant_unref(bgdata->params);
                }
            }
            catch (const std::exception &excp)
            {
                sd_resolved_bg_log("NetCfg::DNS::resolved::Link::BackgroundCall - LAMBDA] Preparation EXCEPTION: {}",
                                   excp.what());
                if (bgdata->error_callback)
                {
                    error_messages.push_back(std::string(excp.what()));
                    bgdata->error_callback(error_messages);
                }
            }
            (*running_tasks_count)--;
        });
}


//
//  NetCfg::DNS::resolved::Manager
//


Manager::Ptr Manager::Create(DBus::Connection::Ptr conn)
{
    return Manager::Ptr(new Manager(std::move(conn)));
}


Manager::Manager(DBus::Connection::Ptr conn)
{
    proxy = DBus::Proxy::Client::Create(conn, "org.freedesktop.resolve1");
    tgt_resolved = DBus::Proxy::TargetPreset::Create(
        "/org/freedesktop/resolve1", "org.freedesktop.resolve1.Manager");

    // Check for presence of org.freedesktop.PolicyKit1
    // This service is needed to be allowed to send update requests
    // to systemd-resolved as the 'openvpn' user which net.openvpn.v3.netcfg
    // run as
    try
    {
        auto prxsrv = DBus::Proxy::Utils::DBusServiceQuery::Create(conn);
        if (prxsrv->StartServiceByName("org.freedesktop.PolicyKit1") < 1)
        {
            throw DBus::Exception(__func__, "");
        }

        std::string n = prxsrv->GetNameOwner("org.freedesktop.PolicyKit1");
        if (n.empty())
        {
            throw DBus::Exception(__func__, "");
        }
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception(std::string("Could not access ")
                        + "org.freedesktop.PolicyKit1 (polkitd) service. "
                        + "Cannot configure systemd-resolved integration");
    }

    //  Start the a background thread responsible for executing
    //  some selected D-Bus calls to the systemd-resolved in the
    //  background.  This is to avoid various potential timeouts in
    //  calls where there is little value to wait for a reply.
    asio_errors = Error::Storage::Create();
    asio_keep_running = true;
    async_proxy_thread = std::async(
        std::launch::async,
        [&]()
        {
            asio::executor_work_guard<asio::io_context::executor_type> worker = asio::make_work_guard(asio_proxy);

            while (asio_keep_running)
            {
                try
                {
                    sd_resolved_debug("resolved::Manager() async_proxy_thread - asio::io_context::run() started - asio_keep_running={}",
                                      asio_keep_running);
                    asio_proxy.run();
                    sd_resolved_debug("resolved::Manager() async_proxy_thread - asio::io_context::run() completed - asio_keep_running={}",
                                      asio_keep_running);
                }
                catch (const std::exception &excp)
                {
                    sd_resolved_bg_log("[resolved::Manager() async_proxy_thread] Exception: {}",
                                       excp.what());
                }
            }
            sd_resolved_debug("resolved::Manager() async_proxy_thread - stopping asio::io_context - asio_keep_running={}",
                              asio_keep_running);
        });
}


Manager::~Manager() noexcept
{
    asio_keep_running = false;
    if (!asio_proxy.stopped())
    {
        asio_proxy.stop();
    }
    if (async_proxy_thread.valid())
    {
        async_proxy_thread.get();
    }
}


Link::Ptr Manager::RetrieveLink(const std::string &dev_name)
{
    unsigned int if_idx = ::if_nametoindex(dev_name.c_str());
    if (0 == if_idx)
    {
        throw Exception(fmt::format(
            FMT_COMPILE("Could not retrieve if_index for '{}': {}"),
            dev_name,
            ::strerror(errno)));
    }
    auto link_path = GetLink(if_idx);
    if (link_path.empty())
    {
        return nullptr;
    }
    return Link::Create(asio_proxy, asio_errors, proxy, if_idx, link_path, dev_name);
}


DBus::Object::Path Manager::GetLink(int32_t if_idx) const
{
    GVariant *res = proxy->Call(tgt_resolved,
                                "GetLink",
                                glib2::Value::Create(if_idx));
    glib2::Utils::checkParams("GetLink", res, "(o)", 1);
    try
    {
        auto link_path = glib2::Value::Extract<DBus::Object::Path>(res, 0);
        g_variant_unref(res);
        return link_path;
    }
    catch (const DBus::Exception &excp)
    {
        throw Exception(fmt::format(
            FMT_COMPILE("Could not retrieve systemd-resolved path for if_index {}: {}"),
            if_idx,
            excp.what()));
    }
}

} // namespace resolved
} // namespace DNS
} // namespace NetCfg
