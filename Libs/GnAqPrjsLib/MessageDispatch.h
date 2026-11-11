// MessageDispatch.h, Version: 1.04
#pragma once
#include <GnBasics.h>
#include <Messages.h>
#include <GnUdpRxCounters.h>
#include <utility>

inline TMsgType MessageType(TGnWireValue Packet)
 {
  TMsgType Type=TMsgType::Unknown;TGNString Error;
  return Packet.Object() && GnRead(Packet.Get("Type"),Type,Error) ? Type : TMsgType::Unknown;
 }

// Dispatch checks the transport envelope only. A NULL application value reaches
// its handler, which decides whether that value is meaningful for the operation.
inline bool MessageEnvelopeValid(TGnWireValue Packet,TMsgType Type,[[maybe_unused]] bool CountInvalid=false)
 {
  switch(Type)
   {
#define JD(...)
#if GN_DEBUG_UDP_RX
  #define GN_COUNT_INVALID(Valid) if(CountInvalid && !(Valid))++GnUdpInvalidContentCount;
#else
  #define GN_COUNT_INVALID(Valid)
#endif
#define JD_MESSAGE(Name, Tag, Value) case TMsgType::Tag: { const bool Valid=GnTypeValid(Packet,TGnJsonType<Name>{}); GN_COUNT_INVALID(Valid) return Valid; }
#if GN_FEEDER_UDP
  #define Do_Feeder_RX
#else
  #define Do_Device_RX
#endif
#include "MessageDefinition.inc.h"
#include "GnJsonMacroCleanup.inc.h"
#undef GN_COUNT_INVALID
    default:return false;
   }
 }

inline bool MessageEnvelopeValid(TGnWireValue Packet)
 {return MessageEnvelopeValid(Packet,MessageType(Packet));}

inline bool ValidateReceivedMessage(TGnWireValue Packet,TMsgType Type)
 {
  return MessageEnvelopeValid(Packet,Type,true);
 }

template<class TRx,class... TArgs>
bool DispatchMessage(TGnWireValue Packet,TRx & Receiver,TArgs &&... Args)
 {
  const auto Type=MessageType(Packet);
  if(!ValidateReceivedMessage(Packet,Type))return false;
  switch(Type)
   {
#define JD(...)
#define JD_MESSAGE(Name, Tag, Value) \
    case TMsgType::Tag: \
      if constexpr(requires { Receiver.RxMsg_##Tag(Packet,std::forward<TArgs>(Args)...); }) \
       { \
        Receiver.RxMsg_##Tag(Packet,std::forward<TArgs>(Args)...); \
        return true; \
       } \
      else return false;
#if GN_FEEDER_UDP
  #define Do_Feeder_RX
#else
  #define Do_Device_RX
#endif
#include "MessageDefinition.inc.h"
#include "GnJsonMacroCleanup.inc.h"
    default:return false;
   }
 }
