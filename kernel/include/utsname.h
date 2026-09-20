#ifndef __UTSNAME_H
#define __UTSNAME_H

struct utsname {
    char sysname[65];     // Operating system name
    char nodename[65];    // Hostname of this machine
    char release[65];     // Operating system release
    char version[65];     // Operating system version
    char machine[65];     // Hardware architecture
    char domainname[65];  // Network domain name
};


#endif
