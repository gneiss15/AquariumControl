// GnUdpSchema.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <Messages.h>

template<class T> void GnInitMessage(TGnWireValue Output,TGnJsonType<T>)
 {
  std::apply([&](auto const &... Field)
   {((Field.Initialize?Field.Initialize(Output,Field.Name):void()),...);},GnJsonFields(TGnJsonType<T>{}));
 }

#include "GnUdpDispatch.h"

template<class T> char const * GnJsonFieldPart(TGNString const & Name)
 {
  char const * Part=nullptr;
  if constexpr(T::JsonShape==TGnJsonShape::Object)
    std::apply([&](auto const &... Field)
     {((Name==Field.Name?void(Part=Field.Part):void()),...);},GnJsonObjectFields<T>());
  return Part;
 }
