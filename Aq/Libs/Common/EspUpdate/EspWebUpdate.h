// EspWebUpdate.h, Version: 1.20
#pragma once

#include <GnBasics.h>

#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #include <Update.h>
  using TEspUpdateHttpServer = WebServer;
#else
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
  #include <ESP8266WebServer.h>
  #include <Updater.h>
  #include <Updater_Signing.h>
  extern "C"
   {
    #include <eboot_command.h>
   }
  using TEspUpdateHttpServer = ESP8266WebServer;
#endif

#include <LittleFS.h>
#include <memory>
#include <new>
#include <vector>

class TEspWebUpdate
 {
 public:
                              TEspWebUpdate(void);

#ifdef ESP8266
  void                        Setup( TEspUpdateHttpServer & HttpServer,
                                    UpdaterHashClass * SigningHash,
                                    UpdaterVerifyClass * SigningVerifier );
  void                        SetupRecovery( TEspUpdateHttpServer & HttpServer,
                                    UpdaterHashClass * SigningHash,
                                    UpdaterVerifyClass * SigningVerifier,
                                    char const * ProjectName );
  void                        SetupStartupError( TEspUpdateHttpServer & HttpServer,
                                    UpdaterHashClass * SigningHash,
                                    UpdaterVerifyClass * SigningVerifier,
                                    char const * ProjectName,
                                    String const & Error );
#else
  void                        Setup( TEspUpdateHttpServer & HttpServer );
  void                        SetupRecovery( TEspUpdateHttpServer & HttpServer,
                                    char const * ProjectName );
  void                        SetupStartupError( TEspUpdateHttpServer & HttpServer,
                                    char const * ProjectName,
                                    String const & Error );
#endif

 private:
  using TPackageRamChunks = std::vector<std::unique_ptr<uint8_t[]>>;

  enum class TPart : uint8_t
   {
    None,
    Firmware,
    FileSystem,
    CombinedPackage,
    Config
   };

  enum class TPackageState : uint8_t
   {
    Header,
    TableFileHeader,
    TablePath,
    CompareData,
    StageData,
    Done
   };

  enum class TCombinedPackageState : uint8_t
   {
    Header,
    Firmware,
    FileSystem,
    Done
   };

  struct TPackageEntry
   {
    String                     Name;
    uint32_t                   DataSize;
    uint32_t                   DataWritten;
    size_t                     RamOffset;
    size_t                     TemporaryDataSize;
    bool                       Identical;
   };

  TEspUpdateHttpServer *       FHttpServer;
#ifdef ESP8266
  UpdaterHashClass *           FSigningHash;
  UpdaterVerifyClass *         FSigningVerifier;
#endif
  String                       FError;
  TPart                        FCurrentPart;
  TPackageState                FPackageState;
  TCombinedPackageState        FCombinedPackageState;
  bool                         FCombinedRequest;
  bool                         FCombinedPackageUpload;
  bool                         FCombinedPackageVerificationActive;
  bool                         FCombinedPackageVerified;
  bool                         FConfigCompleted;
  bool                         FConfigVerificationActive;
  bool                         FFileSystemCompleted;
  bool                         FFileSystemUnmounted;
  bool                         FFileSystemVerificationActive;
  bool                         FFirmwareVerificationActive;
  bool                         FFirmwareCompleted;
  bool                         FFirmwareStaged;
  bool                         FProgressResponseActive;
  bool                         FRequestActive;
  bool                         FRecoveryMode;
  String                       FRescueProjectName;
  String                       FStartupError;
  std::unique_ptr<uint8_t[]>   FSignatureTail;
  size_t                       FSignatureTailCapacity;
  size_t                       FSignatureTailSize;
  size_t                       FFirmwarePayloadSize;
  size_t                       FFirmwareExpectedPayloadSize;
  size_t                       FConfigExpectedPayloadSize;
  TGNString                    FConfigUploadData;
  uint8_t                      FDeviceTypeHeader[64];
  size_t                       FDeviceTypeHeaderSize;
  uint32_t                     FDeviceTypePayloadSize;
  uint32_t                     FDeviceTypePayloadRemaining;
  bool                         FDeviceTypeHeaderVerified;
  bool                         FUpdatePayloadStarted;
  unsigned                     FUploadedFileCount;
  uint8_t                      FCombinedPackageHeader[12];
  size_t                       FCombinedPackageHeaderSize;
  uint32_t                     FCombinedFirmwareRemaining;
  uint8_t                      FPackageHeader[18];
  size_t                       FPackageHeaderSize;
  uint8_t                      FPackageFileHeader[5];
  size_t                       FPackageFileHeaderSize;
  char                         FPackagePath[32];
  size_t                       FPackagePathLength;
  size_t                       FPackagePathSize;
  uint32_t                     FPackageTotalDataLength;
  uint32_t                     FPackageReceivedDataLength;
  uint16_t                     FPackageFileCount;
  uint16_t                     FPackageTableIndex;
  uint16_t                     FPackageDataIndex;
  size_t                       FPackageChangedFileCount;
  size_t                       FPackageDeletedFileCount;
  size_t                       FPackageCommittedFileCount;
  File                         FPackageTargetFile;
  std::vector<TPackageEntry>   FPackageEntries;
  TPackageRamChunks            FPackageRamChunks;
  size_t                       FPackageRamDataSize;

  bool                         AllocatePackageRam( size_t Size );
  void                         AbortCurrentUpdate(void);
  bool                         BeginCombinedPackageUpload(void);
  bool                         BeginConfigUpload( size_t SignedFileSize );
  bool                         BeginFileSystemUpload(void);
  bool                         BeginFileSystemPayloadUpload(void);
  bool                         BeginFirmwareUpload( size_t SignedFileSize );
  bool                         BeginFirmwarePayloadUpload( size_t PayloadSize );
  void                         BeginProgressResponse(void);
  void                         BeginRequest( bool CombinedRequest );
  bool                         BuildPackageStoragePlan(void);
  void                         CancelPendingFirmware(void);
  bool                         CheckDeviceTypePayloadComplete( TPart Part );
  bool                         CheckFileName( String const & FileName, TPart Part ) const;
  bool                         ConsumeDeviceTypeHeader( TPart Part,
                                    uint8_t const * & Data, size_t & Size );
  bool                         CleanupTemporaryPackageFiles(void);
  bool                         CompletePackageComparePass(void);
  bool                         CommitPackageFiles(void);
  bool                         CommitRamPackageFiles(void);
  bool                         CommitTemporaryPackageFiles(void);
  bool                         CopyToPackageRam( size_t Offset, uint8_t const * Data, size_t Size );
  bool                         CompleteCombinedPackageUpdate(void);
  void                         CompleteRequest( bool CombinedRequest );
  String                       CreateResponsePage( bool Ok, bool CombinedRequest ) const;
  bool                         DeleteObsoletePackageFiles(void);
  bool                         EndCombinedPackageUpload(void);
  bool                         EndConfigUpload(void);
  bool                         EndFileSystemUpload(void);
  bool                         EndFirmwareUpload(void);
  void                         EndProgressResponse(void);
  static String                EscapeHtml( String const & Text );
  size_t                       GetMaximumSize( TPart Part ) const;
  String                       GetTemporaryPackageFileName( String const & FileName ) const;
  void                         HandleUpload( bool CombinedRequest );
  bool                         IsPackageFileName( String const & FileName ) const;
  bool                         ProcessCombinedPackagePayload( uint8_t const * Data, size_t Size );
  bool                         ProcessPackageData( uint8_t const * Data, size_t Size );
  void                         ReportPackageFileCommitted(void);
  void                         ResetCombinedPackageVerification(void);
  void                         ResetConfigVerification(void);
  void                         ResetDeviceTypeHeader(void);
  void                         ResetPackageRam(void);
  void                         ResetFileSystemVerification(void);
  void                         ResetFirmwareVerification(void);
  void                         ResetPackageParser(void);
  void                         ResetRequest(void);
  void                         ResetSignatureTail(void);
  void                         SetError( String const & Error );
  void                         SetError( __FlashStringHelper const * Error );
  void                         SetUpdaterError( String const & Prefix );
  void                         SetUpdaterError( __FlashStringHelper const * Prefix );
  void                         SendProgressError(void);
  void                         SendProgressLine( String const & Line );
  void                         SendRecoveryPage(void);
  void                         SendStartupErrorPage(void);
  bool                         StartPackageCompareEntry(void);
  bool                         StartPackageStageEntry(void);
  void                         StartUpload( HTTPUpload const & Upload, bool CombinedRequest );
  bool                         WriteCombinedPackageData( uint8_t const * Data, size_t Size );
  bool                         WriteConfigData( uint8_t const * Data, size_t Size );
  bool                         WriteConfigPayload( uint8_t const * Data, size_t Size );
  bool                         WriteCombinedPackagePayload( uint8_t const * Data, size_t Size );
  bool                         WriteFileSystemData( uint8_t const * Data, size_t Size );
  bool                         WriteFileSystemPayload( uint8_t const * Data, size_t Size );
  bool                         WriteFirmwareData( uint8_t const * Data, size_t Size );
  bool                         WriteFirmwarePayload( uint8_t const * Data, size_t Size );
  bool                         WritePackageRamToFile( File & TargetFile, size_t Offset, size_t Size );
 };
