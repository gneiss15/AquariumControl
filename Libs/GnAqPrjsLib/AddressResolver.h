// AddressResolver.h, Version: 1.00
#pragma once
#include <GnBasics.h>
#include <ChannelAddress.h>
#include <vector>

// Only known physical endpoints participate in resolution. Omitted service or
// channel components are accepted when they still select one unique endpoint.
struct TPhysicalEndpoint
 {
  TGNString Address;
  uint32_t Capabilities=0;
 };

inline bool ResolveChannelAddress(TGNString const & Input,
                                 std::vector<TPhysicalEndpoint> const & Endpoints,
                                 uint32_t RequiredCapabilities,TGNString & Output)
 {
  TChannelAddress Requested;
  if(!TChannelAddress::Parse(Input,Requested))return false;
  TGNString Match;
  for(auto const & Endpoint:Endpoints)
   {
    if((Endpoint.Capabilities & RequiredCapabilities)!=RequiredCapabilities)continue;
    TChannelAddress Candidate;
    if(!TChannelAddress::Parse(Endpoint.Address,Candidate) || !Candidate.HasService || !Candidate.HasChannel)continue;
    if(Candidate.Mac!=Requested.Mac ||
       (Requested.HasService && Candidate.Service!=Requested.Service) ||
       (Requested.HasChannel && Candidate.Channel!=Requested.Channel))continue;
    const TGNString Canonical=Candidate.Mac+"@"+std::to_string(Candidate.Service)+"/"+std::to_string(Candidate.Channel);
    if(!Match.empty() && Match!=Canonical)return false;
    Match=Canonical;
   }
  if(Match.empty())return false;
  Output=std::move(Match);return true;
 }

inline TGNString ServiceAddress(TChannelAddress const & Address)
 {
  return Address.HasService ? Address.Mac+"@"+std::to_string(Address.Service) : TGNString{};
 }
