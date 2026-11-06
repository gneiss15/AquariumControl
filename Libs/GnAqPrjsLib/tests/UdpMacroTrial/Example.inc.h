// Example.inc.h, Version: 1.02
// Prototype only. Deliberately included repeatedly without an include guard.
#ifndef JD_BEGIN
#error Define JD_BEGIN before including this file
#endif
#ifndef JD_CONST
#error Define JD_CONST before including this file
#endif
#ifndef JD
#error Define JD before including this file
#endif
#ifndef JD_END
#error Define JD_END before including this file
#endif


// Select individual messages or direction groups before the include.
#ifdef Do_All
#define DoMsg_InfoRequest
#define DoMsg_Info
#define Do_TestTypes
#undef Do_All
#endif
#ifdef Do_Observer_TX
#define DoMsg_InfoRequest
#undef Do_Observer_TX
#endif
#ifdef Do_Observer_RX
#define DoMsg_Info
#undef Do_Observer_RX
#endif
#ifdef Do_Service_TX
#define DoMsg_Info
#undef Do_Service_TX
#endif
#ifdef Do_Service_RX
#define DoMsg_InfoRequest
#undef Do_Service_RX
#endif

#ifdef DoMsg_InfoRequest
JD_BEGIN( TGnUdpInfoRequest, "Request missing descriptions" )
JD_CONST( uint32_t, Version, 2 )
JD_CONST( TGNString, Type, "InfoRequest" )
JD( TGNString, ID, Required, WireId, "Physical MAC@ServiceId" )
JD( TGNString, InfoRevision, Required, Hex16, "Description revision" )
JD( TStringList, Fields, Required, Count(1,64), "Requested description fields" )
JD_END( TGnUdpInfoRequest )
#undef DoMsg_InfoRequest
#endif

#ifdef DoMsg_Info
JD_BEGIN( TGnUdpInfo, "Description fragment" )
JD_CONST( uint32_t, Version, 2 )
JD_CONST( TGNString, Type, "Info" )
JD( TGNString, ID, Required, WireId, "Physical MAC@ServiceId" )
JD( TGNString, InfoRevision, Required, Hex16, "Description revision" )
JD( TGNString, Field, Required, DescriptionField, "Description field" )
JD( uint32_t, Chunk, Required, ChunkIndex, "Zero-based fragment index" )
JD( uint32_t, Chunks, Required, Range(1,64), "Fragment count" )
JD( TGNString, Data, Required, Utf8Bytes(400), "Fragment of serialized JSON" )
JD_END( TGnUdpInfo )
#undef DoMsg_Info
#endif

#ifdef Do_TestTypes
// Test-only types exercise nested objects, arrays, null and optional fields.
JD_BEGIN( TExampleMeasurement, "Test fixture, not a new UDP packet" )
JD( TGNString, Id, Required, NonEmpty, "Measurement identifier" )
JD( double, Value, RequiredNullable, Finite, "A value or explicit null" )
JD( bool, Valid, Required, Any, "Validity flag" )
JD_END( TExampleMeasurement )

JD_BEGIN( TExampleStatus, "Test fixture, not a new UDP packet" )
JD( TExampleMeasurement, Measurement, Required, Any, "Nested measurement" )
JD( TMeasurementList, Measurements, Optional, Count(0,64), "Optional array" )
JD( double, Temperature, OptionalNullable, Finite, "Missing, null or value" )
JD_END( TExampleStatus )

#undef Do_TestTypes
#endif

#undef JD_BEGIN
#undef JD_CONST
#undef JD
#undef JD_END
