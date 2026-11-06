// GnConfigurationJournal.h, Version: 1.01
#pragma once
#include <GnJsonSchema.h>

inline bool GnReadConfigurationJournal(TGNString const & Text, bool HasIntervals,
                                       TJsonConfigurationFiles & Journal)
 {
  TGnWireDocument Document(Text);
  auto Root=Document.Root();
  const size_t Count=HasIntervals?3:2;
  return Root.Object() && Root.Size()==Count && GnReadJsonValue(Text,Journal) &&
    Journal.Chs.Known() && Journal.Schedule.Known() &&
    (HasIntervals?Journal.Intervals.Known():!Journal.Intervals.Present);
 }
