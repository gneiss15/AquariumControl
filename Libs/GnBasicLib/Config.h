#pragma once

//  General Flags
//#define _GNU_SOURCE   // In Version 3.0.1 schon in Command-Line enthalten1

// DEBUG & ENABLEDEBUG
#ifdef DEBUG             // Compat: Old Code uses #define DEBUG instead of ENABLEDEBUG 1
  #undef ENABLEDEBUG
  #define ENABLEDEBUG 1
#endif
#ifndef ENABLEDEBUG
  #define ENABLEDEBUG 1  // ENABLEDEBUG: sould be used for Code that shoud only be compiled for Debug-Builds
#endif

// FINAL_VERSION & FINALVERSION
#ifdef FINAL_VERSION     // Compat: Old Code uses #define FINAL_VERSION instead of FINALVERSION 1
  #undef FINALVERSION
  #define FINALVERSION 1
#endif
#ifndef FINALVERSION
  #define FINALVERSION 0 // FINALVERSION: sould be used for Code that shoud only be compiled for Final-Builds
#endif

