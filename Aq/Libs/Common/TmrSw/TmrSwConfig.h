// TmrSwConfig.h, Version: 3.00
#ifndef TmrSwConfigH
#define TmrSwConfigH

#include "TmrSwIncludes.h"

typedef struct
 {
  #define CD( htmlType, typ, name, defValue, ... ) typ name;
  #include "TmrSwConfig.inc.h"
 }
 TConfig;

class TTmrSwConfig : public TSingleton<TTmrSwConfig>
 {
  friend class TSingleton<TTmrSwConfig>;
 private:
                      TTmrSwConfig(void);
 public:
  TConfig             FConfig;
 private:
  bool                Load( String const jsonStr );
 public:
  bool                Setup(void);
  bool                Save(void);
  bool                Set( String const jsonStr );
  void                Reset(void);
 };
#define TmrSwConfig TTmrSwConfig::Instance().FConfig
#define TmrSwConfigInstance TTmrSwConfig::Instance()

#endif
