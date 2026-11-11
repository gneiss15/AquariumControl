// FeederRestartLog.h, Version: 1.00
#pragma once

void FeederRestartLogSetup( bool Mounted );
bool FeederRestartLogLoop( bool TimeValid );

inline constexpr char FeederRestartLogPath[] = "/diagnostics/restarts.csv";
inline constexpr char FeederRestartLogPreviousPath[] = "/diagnostics/restarts.previous.csv";
