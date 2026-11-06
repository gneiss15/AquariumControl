// GnUdpDispatch.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <MessageDispatch.h>

using TGnUdpMessageType=TMsgType;
inline TMsgType GnUdpMessageType(TGnWireValue Input) {return MessageType(Input);}
inline bool GnValidateUdpMessage(TGnWireValue Input) {return MessageEnvelopeValid(Input);}

template<class T> TGNString GnWriteUdpMessage(T const & Value)
 {
  TGnWireDocument Document;
  GnStore(Document.Root(),"Packet",Value);
  auto Packet=Document.Root().Get("Packet");
  return GnTypeValid(Packet,TGnJsonType<T>{})?Packet.Json():TGNString{};
 }

template<class TRx,class... TArgs>
bool GnDispatchUdpMessage(TGnWireValue Input,TRx & Receiver,TArgs &&... Args)
 {return DispatchMessage(Input,Receiver,std::forward<TArgs>(Args)...);}
