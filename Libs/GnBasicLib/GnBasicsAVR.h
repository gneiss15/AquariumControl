// GnBasicsAVR.h, Version: 1.01
#pragma once

//****************************************************************
// Includes
//****************************************************************

#include <Arduino.h>
#include <Config.h>

#include <stddef.h>
#include <stdint.h>

//****************************************************************
// Global Types
//****************************************************************

typedef unsigned int uint_t;

//****************************************************************
// Global Macros
//****************************************************************

#define MakeStr( a )                  MakeStr_( a )
#define MakeStr_( a )                 #a

#define EmptyMacro()
#define MacroFunc( exp )              do exp while( 0 )

#define noreturn_                     __attribute__(( noreturn ))
#define packed_                       __attribute__((packed))
#define align_( size )                __attribute__((aligned(size)))
#define unused_( x )                  x __attribute__((unused))
#define unused_para( def, var )       def var __attribute__((unused))

//****************************************************************
// Functions (Time)
//****************************************************************

static inline void DelayUs( uint32_t Us )                                     { delayMicroseconds( Us ); }
static inline void DelayMs( uint32_t Ms )                                     { delay( Ms ); }
static inline void DelayS( uint32_t Seconds )                                 { while( Seconds-- ) delay( 1000 ); }
static inline uint32_t GnMicros(void)                                         { return micros(); }
static inline uint32_t GnMillis(void)                                         { return millis(); }

//****************************************************************
// Functions (GPIO)
//****************************************************************

#define PinMode_Output( pinNr )                              pinMode( pinNr, OUTPUT )
#define PinMode_Input( pinNr )                               pinMode( pinNr, INPUT )
#define PinMode_InputPullUp( pinNr )                         pinMode( pinNr, INPUT_PULLUP )

//****************************************************************
// Policy Classes
//****************************************************************

class no_defaultcreate
 {
 protected:
  inline              no_defaultcreate(void) = delete;
 };

class no_copy
 {
 private:
                      no_copy( no_copy const & src ) = delete;
 public:
  inline              no_copy(void) throw() = default;
 };

class no_assign
 {
 private:
  template<class T>
    no_assign &       operator=( T const & rhs ) = delete;
 public:
  inline              no_assign(void) throw() = default;
 };

template <class T>
  class TSingleton : no_copy, no_assign
   {
   protected:
    inline            TSingleton(void) throw() = default;
                      ~TSingleton(void) throw() = default;
   public:
    static inline T & Instance(void);
   };
template<class T>
 /*static*/ T &       TSingleton<T>::Instance(void)
  {
   static T SInstance;
   return SInstance;
  }

//****************************************************************
// TLoopInstances
//****************************************************************

class TLoopInstances;
class TLoopInstance;

class TLoopInstances : public TSingleton<TLoopInstances>
 {
  friend class TSingleton<TLoopInstances>;
  friend class TLoopInstance;
 protected:
  TLoopInstance *     FList;
 private:
                      TLoopInstances(void)              : FList( NULL ) {}
                      ~TLoopInstances(void)             { FList = NULL; }
 protected:
  void                Add( TLoopInstance * obj );
  void                Remove( TLoopInstance * obj );
 public:
  void                Loop(void);
 };
#define LoopInstances TLoopInstances::Instance()

//****************************************************************
// TLoopInstance
//****************************************************************

class TLoopInstance
 {
  friend class TLoopInstances;
 private:
  virtual void        Loop(void)                        = 0;
  TLoopInstance *     FNext;
 public:
  TLoopInstance *     Next(void)                        { return FNext; }

 public:
                      TLoopInstance(void)               : FNext( NULL ) { LoopInstances.Add( this ); }
                      ~TLoopInstance(void)              { LoopInstances.Remove( this ); }
 };

inline void TLoopInstances::Add( TLoopInstance * obj )
 {
  obj->FNext = FList;
  FList = obj;
 }

inline void TLoopInstances::Remove( TLoopInstance * obj )
 {
  for( TLoopInstance ** p = &FList;  *p;  p = &( (*p)->FNext ) )
    if( *p == obj )
     {
      *p = obj->Next();
      return;
     }
 }

inline void TLoopInstances::Loop(void)
 {
  for( TLoopInstance * p = FList;  p;  p = p->FNext )
    p->Loop();
 }

//****************************************************************
// Debug / output
//****************************************************************

#define Dprintln(...)
#define Dprintf(...)
#define Eprintln(...)
#define Eprintf(...)
#define Println(...)
#define Printf(...)
#define Debug(...)
