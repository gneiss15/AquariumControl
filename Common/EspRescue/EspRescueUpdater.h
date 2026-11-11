// EspRescueUpdater.h, Version: 2.09
#pragma once

#include <atomic>
#include <cstddef>
#include <freertos/FreeRTOS.h>
#include <cstdint>

#include <esp_ota_ops.h>
#include <esp_partition.h>

class TEspRescueUpdater
 {
 public:
                              TEspRescueUpdater(void);
  bool                        Setup( char const * ProjectName,
                                    char const * RescueVersion );
  void                        Loop(void);

 private:
  enum class TPart : uint8_t
   {
    Firmware,
    FileSystem
   };

  int                         FServerSocket;
  char const *                FProjectName;
  char const *                FRescueVersion;
  esp_partition_t const *     FMainPartition;
  esp_partition_t const *     FFileSystemPartition;
  std::atomic<int>            FUploadSocket{-1};
  TPart                       FUploadPart = TPart::Firmware;
  uint32_t                    FUploadLength = 0;
  bool                        FClientTransferred = false;
  portMUX_TYPE                FProgressLock = portMUX_INITIALIZER_UNLOCKED;
  const char *                FPhase = "Idle";
  char                        FLastError[160] = {};
  uint32_t                    FWritten = 0;
  uint32_t                    FTotal = 0;
  bool                        FUpdateRequest = false;
  bool                        StartUpload( int ClientSocket, TPart Part, uint32_t ContentLength );
  static void                 UploadTask( void * Context );
  void                        SetPhase( const char * Phase );
  void                        SetWritten( uint32_t Written );
  void                        SendProgress( int ClientSocket );
  void                        SendEspError( int ClientSocket, const char * Text, esp_err_t Error );
  bool                        FUpdateCompleted;
  bool                        FMainWriteUnverified = false;
  bool                        FFileSystemWriteUnverified = false;

  bool                        HandleClient( int ClientSocket );
  bool                        HandleUpdate( int ClientSocket,
                                    TPart Part, uint32_t ContentLength );
  bool                        HandleFirmware( int ClientSocket,
                                    uint32_t PayloadSize, uint8_t const ExpectedHash[32] );
  bool                        HandleFileSystem( int ClientSocket,
                                    uint32_t PayloadSize, uint8_t const ExpectedHash[32] );
  void                        HandleCommit( int ClientSocket );
  void                        HandleMain( int ClientSocket );
  bool                        ReadHeaders( int ClientSocket,
                                    uint32_t & ContentLength );
  bool                        ReadLine( int ClientSocket, char * Buffer,
                                    size_t BufferSize,
                                    bool DiscardOverflow = false );
  bool                        ReadExact( int ClientSocket, uint8_t * Buffer,
                                    size_t Size );
  bool                        SendAll( int ClientSocket, char const * Text );
  bool                        SendAll( int ClientSocket, void const * Data,
                                    size_t Size );
  void                        RestoreRescueBoot(void);
  void                        SendRecoveryPage( int ClientSocket );
  void                        SendText( int ClientSocket, int Status,
                                    char const * Text );
 };
