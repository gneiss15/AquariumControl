// ChannelAddress.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <string_view>

// MAC [ @ ServiceId ] [ / ChannelNr ]. Resolution of omitted components
// belongs to the receiver: a command must select exactly one physical output.
struct TChannelAddress
 {
  TGNString Mac;
  uint32_t Channel=0;
  uint8_t Service=0;
  bool HasChannel=false;
  bool HasService=false;

  static bool Parse(TGNString const & Text,TChannelAddress & Result)
   {
    std::string_view Value(Text.data(),Text.size());
    if(Value.size()<17)return false;
    TChannelAddress Next;
    bool Nonzero=false;
    for(size_t I=0;I<17;++I)
     {
      const char C=Value[I];
      if(I%3==2) { if(C!=':')return false; }
      else
       {
        if(!((C>='0' && C<='9') || (C>='A' && C<='F') || (C>='a' && C<='f')))return false;
        Nonzero=Nonzero || C!='0';
       }
     }
    if(!Nonzero)return false;
    Next.Mac=Text.substr(0,17);
    for(char & C:Next.Mac)if(C>='a' && C<='f')C=char(C-'a'+'A');
    size_t At=17;
    if(At<Value.size() && Value[At]=='@')
     {
      ++At;
      if(At==Value.size() || Value[At]<'1' || Value[At]>'4')return false;
      Next.Service=uint8_t(Value[At++]-'0');Next.HasService=true;
     }
    if(At<Value.size() && Value[At]=='/')
     {
      ++At;
      if(At==Value.size())return false;
      Next.HasChannel=true;
      for(;At<Value.size();++At)
       {
        const char C=Value[At];
        if(C<'0' || C>'9')return false;
        const uint32_t Digit=uint32_t(C-'0');
        if(Next.Channel>(UINT32_MAX-Digit)/10)return false;
        Next.Channel=Next.Channel*10+Digit;
       }
     }
    if(At!=Value.size())return false;
    Result=std::move(Next);return true;
   }
 };
