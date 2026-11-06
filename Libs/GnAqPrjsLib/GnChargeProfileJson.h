// GnChargeProfileJson.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <GnJsonSchema.h>

// Compatibility entry point; the schema owns all field and value checks.
inline bool GnChargeProfileJsonValid( json_object * Row )
 {
  return GnTypeValid( TGnWireValue( Row ), TGnJsonType<TJsonProfile>{}, true );
 }
