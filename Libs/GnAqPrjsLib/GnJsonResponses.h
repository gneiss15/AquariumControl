// GnJsonResponses.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <GnJsonSchema.h>

inline TGNString GnJsonResult(bool Ok,TGNString const & Error={},TGNString const & Reason={})
 {
  THttpResult Result;
  Result.Ok=Ok;
  if(!Error.empty())Result.Err=Error;
  if(!Reason.empty())Result.Reason=Reason;
  return GnWriteJson(Result);
 }
inline TGNString GnJsonChangedResult(bool Changed)
 {
  THttpResult Result;
  Result.Ok=true;Result.Changed=Changed;
  return GnWriteJson(Result);
 }
inline TGNString GnJsonPendingResult(bool Pending)
 {
  THttpResult Result;
  Result.Ok=true;Result.Pending=Pending;
  return GnWriteJson(Result);
 }
inline TGNString GnJsonError(TGNString const & Message)
 {
  TUdpLastError Error;
  Error.Message=Message;
  return GnWriteJson(Error);
 }
