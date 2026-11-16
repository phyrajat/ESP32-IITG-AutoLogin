#pragma once

#include <string>

// Requests against the IITG captive portal (agnigarh.iitg.ac.in). All calls
// block on the network and keep no state between calls.
namespace portal {

enum class Probe {
    Internet, // the internet is reachable, no login needed
    Portal,   // the request was intercepted: login required
    Error,    // the request did not complete
};

// Checks whether the captive portal is intercepting traffic.
Probe probe();

// Logs in and returns the keepalive token in `keepaliveToken`.
bool login(const std::string &userId, const std::string &password, std::string &keepaliveToken);

// Refreshes the portal session identified by the token.
bool keepalive(const std::string &keepaliveToken);

// Ends the portal session.
void logout();

} // namespace portal
