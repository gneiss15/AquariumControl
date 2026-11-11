// Messages.h, Version: 1.03
#pragma once
#include <GnBasics.h>
#include <MessageTypes.h>
#include <GnJsonNative.h>

#include "MessageSelection.inc.h"

// A translation unit may select only its messages before this include.
#if !defined(Do_ContractAll) && \
    !defined(Do_THttpButtonAction) && \
    !defined(Do_THttpDevice) && \
    !defined(Do_TJsonSchedule) && \
    !defined(Do_THttpTimePayload) && \
    !defined(Do_THttpTimeReply) && \
    !defined(Do_TUdpPowerMeter) && \
    !defined(Do_TUdpStatusData) && \
    !defined(Do_TUdpStatusMessage) && \
    !defined(Do_TWsStatusEvent) && \
    !defined(Do_TJsonCachedStatus) && \
    !defined(Do_TSseStatusBatch) && \
    !defined(Do_TJsonHistoryEnergyRow) && \
    !defined(Do_TJsonHistorySeries) && \
    !defined(Do_THttpHistoryReply) && \
    !defined(Do_TJsonRegistryView) && \
    !defined(Do_TSseRecordingEvent) && \
    !defined(Do_TJsonFeedTime) && \
    !defined(Do_TJsonFeedTimes) && \
    !defined(Do_TUdpLastError) && \
    !defined(Do_TUdpStateLabels) && \
    !defined(Do_TUdpChannel) && \
    !defined(Do_TJsonRegistryDevice) && \
    !defined(Do_TJsonFeedbackState) && \
    !defined(Do_TJsonFeedbackStates) && \
    !defined(Do_TWsEventEndpoint) && \
    !defined(Do_TJsonEnergyAssignment) && \
    !defined(Do_TJsonHistoryMetadata) && \
    !defined(Do_TJsonFunctionConfig) && \
    !defined(Do_TJsonFunctionEnvelope) && \
    !defined(Do_TJsonSources) && \
    !defined(Do_TJsonInterval) && \
    !defined(Do_TJsonIntervals) && \
    !defined(Do_THttpResult) && \
    !defined(Do_THttpFeedbackWrite) && \
    !defined(Do_TUdpNextChange) && \
    !defined(Do_TUdpChargeLearning) && \
    !defined(Do_TUdpHeatControl) && \
    !defined(Do_TJsonProfiles) && \
    !defined(Do_TJsonHistoryFile) && \
    !defined(Do_THttpHistoryManifest) && \
    !defined(Do_TJsonIdentity) && \
    !defined(Do_TJsonIdentityEnvelope) && \
    !defined(Do_TJsonRecordingChannels) && \
    !defined(Do_TJsonChannel) && \
    !defined(Do_TJsonChannels) && \
    !defined(Do_TJsonChannelGroup) && \
    !defined(Do_TJsonHistorySample) && \
    !defined(Do_TJsonHistoryState) && \
    !defined(Do_TUdpHeatDiagnostics) && \
    !defined(Do_TJsonHeatHistoryConfig) && \
    !defined(Do_TJsonHistoryHeatRow) && \
    !defined(Do_TJsonSwitchTimes) && \
    !defined(Do_TJsonSwitchTime) && \
    !defined(Do_TUdpRequestMessage) && \
    !defined(Do_THttpFeedbackDocument) && \
    !defined(Do_TJsonConfigurationFiles) && \
    !defined(Do_TJsonFeedbackRule) && \
    !defined(Do_TJsonFeedbackConfiguration) && \
    !defined(Do_TJsonHeatConfig) && \
    !defined(Do_TJsonProfile) && \
    !defined(Do_TJsonBatteryLearningState) && \
    !defined(Do_TUdpSourceInfoMessage) && \
    !defined(Do_TJsonHistoryTemperatureRow) && \
    !defined(Do_TUdpSourceRequestMessage) && \
    !defined(Do_TUdpMeasurement) && \
    !defined(Do_TUdpInfoMessage) && \
    !defined(Do_TUdpInfoRequestMessage) && \
    !defined(Do_TJsonHistoryEndpoint) && \
    !defined(Do_TJsonSyncMeta) && \
    !defined(Do_TJsonRegistryEndpoint) && \
    !defined(Do_TJsonPowerDiagnostics)
  #error Select the required Do_... definitions before including Messages.h
#endif

// Definitions are expanded by the C++ compiler, never by a C++ generator.
#include "GnJsonCompile.inc.h"

#undef Do_ContractAll
#undef Do_THttpButtonAction
#undef Do_THttpDevice
#undef Do_TJsonSchedule
#undef Do_THttpTimePayload
#undef Do_THttpTimeReply
#undef Do_TUdpPowerMeter
#undef Do_TUdpStatusData
#undef Do_TUdpStatusMessage
#undef Do_TWsStatusEvent
#undef Do_TJsonCachedStatus
#undef Do_TSseStatusBatch
#undef Do_TJsonHistoryEnergyRow
#undef Do_TJsonHistorySeries
#undef Do_THttpHistoryReply
#undef Do_TJsonRegistryView
#undef Do_TSseRecordingEvent
#undef Do_TJsonFeedTime
#undef Do_TJsonFeedTimes
#undef Do_TUdpLastError
#undef Do_TUdpStateLabels
#undef Do_TUdpChannel
#undef Do_TJsonRegistryDevice
#undef Do_TJsonFeedbackState
#undef Do_TJsonFeedbackStates
#undef Do_TWsEventEndpoint
#undef Do_TJsonEnergyAssignment
#undef Do_TJsonHistoryMetadata
#undef Do_TJsonFunctionConfig
#undef Do_TJsonFunctionEnvelope
#undef Do_TJsonSources
#undef Do_TJsonInterval
#undef Do_TJsonIntervals
#undef Do_THttpResult
#undef Do_THttpFeedbackWrite
#undef Do_TUdpNextChange
#undef Do_TUdpChargeLearning
#undef Do_TUdpHeatControl
#undef Do_TJsonProfiles
#undef Do_TJsonHistoryFile
#undef Do_THttpHistoryManifest
#undef Do_TJsonIdentity
#undef Do_TJsonIdentityEnvelope
#undef Do_TJsonRecordingChannels
#undef Do_TJsonChannel
#undef Do_TJsonChannels
#undef Do_TJsonChannelGroup
#undef Do_TJsonHistorySample
#undef Do_TJsonHistoryState
#undef Do_TUdpHeatDiagnostics
#undef Do_TJsonHeatHistoryConfig
#undef Do_TJsonHistoryHeatRow
#undef Do_TJsonSwitchTimes
#undef Do_TJsonSwitchTime
#undef Do_TUdpRequestMessage
#undef Do_THttpFeedbackDocument
#undef Do_TJsonConfigurationFiles
#undef Do_TJsonFeedbackRule
#undef Do_TJsonFeedbackConfiguration
#undef Do_TJsonHeatConfig
#undef Do_TJsonProfile
#undef Do_TJsonBatteryLearningState
#undef Do_TUdpSourceInfoMessage
#undef Do_TJsonHistoryTemperatureRow
#undef Do_TUdpSourceRequestMessage
#undef Do_TUdpMeasurement
#undef Do_TUdpInfoMessage
#undef Do_TUdpInfoRequestMessage
#undef Do_TJsonHistoryEndpoint
#undef Do_TJsonSyncMeta
#undef Do_TJsonRegistryEndpoint
#undef Do_TJsonPowerDiagnostics
