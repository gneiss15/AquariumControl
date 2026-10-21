// Logger.h, Version: 2.37
#pragma once
#include <GnBasics.h>
#include <ctime>
#include <mutex>

enum class TLogLevel : uint8_t
 {
  Info,
  Error
 };

class TLogger : public TSingleton<TLogger>
 {
  friend class TSingleton<TLogger>;

 private:
                          TLogger(void) = default;
  static char const *     LevelText( TLogLevel level )
                           {
                            return level == TLogLevel::Error ? "ERROR" : "INFO";
                           }

 public:
  void                    Init( TGNString fileName );
  void                    Write( TLogLevel level, const TGNString & message );
  bool                    GetLastError( TGNString & message, std::time_t & time ) const;

 private:
  TGNString               FileName;
  TGNString               LastErrorMessage;
  std::time_t             LastErrorTime = 0;
  mutable std::mutex      Mutex;
 };

#define Logger TLogger::Instance()
