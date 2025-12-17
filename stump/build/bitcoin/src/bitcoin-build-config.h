// Copyright (c) 2023-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_CONFIG_H
#define BITCOIN_CONFIG_H

/* Version Build */
#define CLIENT_VERSION_BUILD 0

/* Version is release */
#define CLIENT_VERSION_IS_RELEASE false

/* Major version */
#define CLIENT_VERSION_MAJOR 30

/* Minor version */
#define CLIENT_VERSION_MINOR 99

/* Copyright holder(s) before %s replacement */
#define COPYRIGHT_HOLDERS "The %s developers"

/* Copyright holder(s) */
#define COPYRIGHT_HOLDERS_FINAL "The Bitcoin Core developers"

/* Replacement for %s in copyright holders string */
#define COPYRIGHT_HOLDERS_SUBSTITUTION "Bitcoin Core"

/* Copyright year */
#define COPYRIGHT_YEAR 2025

/* Define if external signer support is enabled */
/* #undef ENABLE_EXTERNAL_SIGNER */

/* Define to 1 to enable tracepoints for Userspace, Statically Defined Tracing
   */
/* #undef ENABLE_TRACING */

/* Define to 1 to enable wallet functions. */
/* #undef ENABLE_WALLET */

/* Define to 1 if you have the declaration of `fork', and to 0 if you don't.
   */
#define HAVE_DECL_FORK 1

/* Define to 1 if '*ifaddrs' are available. */
#define HAVE_IFADDRS 1

/* Define to 1 if you have the declaration of `pipe2', and to 0 if you don't.
   */
#define HAVE_DECL_PIPE2 1

/* Define to 1 if you have the declaration of `setsid', and to 0 if you don't.
   */
#define HAVE_DECL_SETSID 1

/* Define to 1 if fdatasync is available. */
#define HAVE_FDATASYNC 1

/* Define this symbol if the BSD getentropy system call is available with
   sys/random.h */
#define HAVE_GETENTROPY_RAND 1

/* Define this symbol if the Linux getrandom function call is available */
#define HAVE_GETRANDOM 1

/* Define this symbol if you have malloc_info */
#define HAVE_MALLOC_INFO 1

/* Define this symbol if you have mallopt with M_ARENA_MAX */
#define HAVE_MALLOPT_ARENA_MAX 1

/* Define to 1 if O_CLOEXEC flag is available. */
#define HAVE_O_CLOEXEC 1

/* Define this symbol if you have posix_fallocate */
#define HAVE_POSIX_FALLOCATE 1

/* Define this symbol if platform supports unix domain sockets */
#define HAVE_SOCKADDR_UN 1

/* Define this symbol to build code that uses getauxval */
#define HAVE_STRONG_GETAUXVAL 1

/* Define this symbol if the BSD sysctl() is available */
/* #undef HAVE_SYSCTL */

/* Define this symbol if the BSD sysctl(KERN_ARND) is available */
/* #undef HAVE_SYSCTL_ARND */

/* Define to 1 if std::system or ::wsystem is available. */
#define HAVE_SYSTEM 1

/* Define to the address where bug reports for this package should be sent. */
#define CLIENT_BUGREPORT "https://github.com/bitcoin/bitcoin/issues"

/* Define to the full name of this package. */
#define CLIENT_NAME "Bitcoin Core"

/* Define to the home page for this package. */
#define CLIENT_URL "https://bitcoincore.org/"

/* Define to the version of this package. */
#define CLIENT_VERSION_STRING "30.99.0"

/* Define to 1 if strerror_r returns char *. */
#define STRERROR_R_CHAR_P 1

/* Define if dbus support should be compiled in */
/* #undef USE_DBUS */

/* Define if QR support should be compiled in */
/* #undef USE_QRCODE */

#endif //BITCOIN_CONFIG_H
