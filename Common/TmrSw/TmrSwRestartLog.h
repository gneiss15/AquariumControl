// TmrSwRestartLog.h, Version: 1.00
#pragma once

bool TmrSwRestartLogSetup( bool FileSystemMounted );
void TmrSwRestartLogFileSystemMounted( bool Mounted );
bool TmrSwRestartLogLoop( bool TimeValid );

inline constexpr char TmrSwRestartLogPath[] = "/diagnostics/restarts.csv";
inline constexpr char TmrSwRestartLogPreviousPath[] = "/diagnostics/restarts.previous.csv";
