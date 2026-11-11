// MessageSelection.inc.h, Version: 1.01
// Optional role groups expand to ordinary per-message selections. No include guard.

#ifdef Do_Feeder_RX
  #define Do_TUdpRequestMessage
  #define Do_TUdpInfoRequestMessage
  #define Do_TUdpSourceRequestMessage
  #undef Do_Feeder_RX
#endif

#ifdef Do_Feeder_TX
  #define Do_TUdpStatusMessage
  #define Do_TUdpInfoMessage
  #define Do_TUdpSourceInfoMessage
  #undef Do_Feeder_TX
#endif

#ifdef Do_Device_TX
  #define Do_TUdpStatusMessage
  #define Do_TUdpInfoMessage
  #define Do_TUdpSourceInfoMessage
  #define Do_TUdpInfoRequestMessage
  #define Do_TUdpSourceRequestMessage
  #undef Do_Device_TX
#endif

#ifdef Do_Device_RX
  #define Do_TUdpStatusMessage
  #define Do_TUdpRequestMessage
  #define Do_TUdpInfoRequestMessage
  #define Do_TUdpInfoMessage
  #define Do_TUdpSourceRequestMessage
  #define Do_TUdpSourceInfoMessage
  #undef Do_Device_RX
#endif

#ifdef Do_Dashboard_TX
  #define Do_TUdpRequestMessage
  #define Do_TUdpInfoRequestMessage
  #define Do_TUdpSourceInfoMessage
  #undef Do_Dashboard_TX
#endif

#ifdef Do_Dashboard_RX
  #define Do_TUdpStatusMessage
  #define Do_TUdpInfoMessage
  #define Do_TUdpSourceRequestMessage
  #define Do_TUdpSourceInfoMessage
  #undef Do_Dashboard_RX
#endif

#ifdef Do_HeatHistory_TX
  #define Do_TUdpInfoRequestMessage
  #undef Do_HeatHistory_TX
#endif

#ifdef Do_HeatHistory_RX
  #define Do_TUdpStatusMessage
  #define Do_TUdpInfoMessage
  #undef Do_HeatHistory_RX
#endif

#ifdef Do_Laptop_TX
  #define Do_TUdpStatusMessage
  #undef Do_Laptop_TX
#endif

#ifdef DoMsg_Request
  #define Do_TUdpRequestMessage
  #undef DoMsg_Request
#endif

#ifdef DoMsg_Status
  #define Do_TUdpStatusMessage
  #undef DoMsg_Status
#endif

#ifdef DoMsg_InfoRequest
  #define Do_TUdpInfoRequestMessage
  #undef DoMsg_InfoRequest
#endif

#ifdef DoMsg_Info
  #define Do_TUdpInfoMessage
  #undef DoMsg_Info
#endif

#ifdef DoMsg_SourceRequest
  #define Do_TUdpSourceRequestMessage
  #undef DoMsg_SourceRequest
#endif

#ifdef DoMsg_SourceInfo
  #define Do_TUdpSourceInfoMessage
  #undef DoMsg_SourceInfo
#endif
