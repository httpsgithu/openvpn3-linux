//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  SPDX-License-Identifier: AGPL-3.0-only
//
//  Copyright (C) 2017-  OpenVPN Inc <sales@openvpn.net>
//  Copyright (C) 2017-  David Sommerseth <davids@openvpn.net>
//

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <string_view>

#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <termios.h>

#include <glib.h>

#include "build-config.h"
#include "build-version.h"
#include "utils.hpp"

#include <asio/version.hpp>
#include <openvpn/legal/copyright.hpp>
#include <openvpn/common/platform_string.hpp>


#ifndef CONFIGURE_GIT_REVISION
constexpr char package_version[] = PACKAGE_GUIVERSION;
#else
constexpr char package_version[] = "git:" CONFIGURE_GIT_REVISION CONFIGURE_GIT_FLAGS;
#endif



void drop_root()
{
    if (getegid() == 0)
    {
        std::cout << "[INFO] Dropping root group privileges to " << OPENVPN_GROUP << std::endl;

        // Retrieve the group information
        errno = 0;
        struct group *groupinfo = getgrnam(OPENVPN_GROUP);
        if (NULL == groupinfo)
        {
            if (errno == 0)
            {
                throw std::runtime_error("Could not find the group ID for " + std::string(OPENVPN_GROUP));
            }
            throw std::runtime_error("An error occurred while calling getgrnam():"
                                     + std::string(strerror(errno)));
        }
        gid_t gid = groupinfo->gr_gid;
        if (-1 == setresgid(gid, gid, gid))
        {
            throw std::runtime_error("Could not set the new group ID (" + std::to_string(gid) + ") "
                                     + "for the user " + std::string(OPENVPN_GROUP));
        }

        // Remove any potential supplementary groups
        if (-1 == setgroups(0, NULL))
        {
            throw std::runtime_error("Could not remove supplementary groups");
        }
    }

    if (geteuid() == 0)
    {
        std::cout << "[INFO] Dropping root user privileges to " << OPENVPN_USERNAME << std::endl;

        // Retrieve the user information
        struct passwd *userinfo = getpwnam(OPENVPN_USERNAME);
        if (NULL == userinfo)
        {
            if (errno == 0)
            {
                throw std::runtime_error("Could not find the User ID for " + std::string(OPENVPN_USERNAME));
            }
            throw std::runtime_error("An error occurred while calling getgrnam():"
                                     + std::string(strerror(errno)));
        }

        uid_t uid = userinfo->pw_uid;
        if (-1 == setresuid(uid, uid, uid))
        {
            throw std::runtime_error("Could not set the new user ID (" + std::to_string(uid) + ") "
                                     + "for the user " + OPENVPN_USERNAME);
        }
    }
}


std::string get_program_version(const std::string &component)
{
    std::stringstream ver;

    ver << PACKAGE_NAME << " " << package_version;

    //  Simplistic basename() approach, extracting just the filename
    //  of the binary from argv[0]
    ver << " (" << simple_basename(component) << ")"
        << std::endl;

    ver << "ASIO version: " << ASIO_VERSION / 100000 << '.' << ASIO_VERSION / 100 % 1000 << '.' << ASIO_VERSION % 100 << '\n';
#if defined(OPENVPN_TUN_BUILDER_BASE_H)
    ver << ClientAPI::OpenVPNClient::platform() << std::endl;
#else
    ver << openvpn::platform_string();
#if defined(ENABLE_OVPNDCO)
    ver << " [DCO]";
#endif
#if defined(OPENVPN_DEBUG)
    ver << " built on " __DATE__ " " __TIME__;
#endif // OPENVPN_DEBUG
#endif // OPENVPN_TUN_BUILDER_BASE_H
    ver << std::endl
        << openvpn_copyright;

    return ver.str();
}


const char *get_package_version()
{
    return package_version;
}


const std::string get_guiversion()
{
#ifdef CONFIGURE_GIT_REVISION
    return std::string(PACKAGE_NAME)
           + "#git:" + std::string(CONFIGURE_GIT_REVISION CONFIGURE_GIT_FLAGS);
#else
    return std::string(PACKAGE_NAME "/" PACKAGE_GUIVERSION);
#endif
}


void set_console_echo(bool echo)
{
    struct termios console;
    tcgetattr(STDIN_FILENO, &console);
    if (echo)
    {
        console.c_lflag |= ECHO;
    }
    else
    {
        console.c_lflag &= ~ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &console);
}


std::string get_local_tstamp(std::time_t epoch)
{
    if (epoch > 0)
    {
        std::stringstream tstmp;
        tstmp << std::put_time(std::localtime(&epoch), "%F %X");
        return std::string(tstmp.str());
    }
    return "";
}


bool is_colour_terminal()
{
    if (getenv("NO_COLOR"))
    {
        return false;
    }

    if (!((isatty(STDOUT_FILENO) > 0)
          && (isatty(STDERR_FILENO) > 0)))
    {
        return false;
    }

    const char *term_env = getenv("TERM");
    std::string term(term_env ? term_env : "");
    if (term.empty() || term == "dumb")
    {
        return false;
    }
    return getenv("COLORTERM") != 0;
}
