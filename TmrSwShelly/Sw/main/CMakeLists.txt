# CMakeLists.txt, Version: 1.06
set(PRJS "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(PROJECT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(GENERATED "${PROJECT}/generated")
set(LIBRARIES "${PRJS_IDF_DEPENDENCIES}/libraries")
set(SRCS main.cpp "${PROJECT}/Sw/TmrSwHw.cpp" "${PROJECT}/Sw/TmrSwWifi.cpp" "${PROJECT}/Sw/ShellyPowerMeter.cpp" "${PROJECT}/Sw/ShellyMeterPacket.cpp")
foreach(NAME SwitchTimes TmrSwChannels TmrSwConfig TmrSwStatusApi TmrSwTimeSync TmrSwWebSocket)
  list(APPEND SRCS "${PROJECT}/Sw/${NAME}.cpp")
endforeach()
foreach(NAME GnBasics GnTime GnMemory GnCoopTimer GNPlatformESP Esp32ResetReasonName)
  list(APPEND SRCS "${PRJS}/Libs/GnBasicLib/${NAME}.cpp")
endforeach()
foreach(NAME GnHeaterControl GnDeviceIdentity GnUdpStatus GnTimeSync)
  list(APPEND SRCS "${PRJS}/Libs/GnAqPrjsLib/${NAME}.cpp")
endforeach()
list(APPEND SRCS "${PRJS}/Libs/GnFtpServer/MyFtpServer.cpp"
  "${PRJS}/Common/EspUpdate/EspSignedArtifact.cpp" "${PRJS}/Common/EspUpdate/EspIndexHtmlCheck.cpp")
file(GLOB FTP_SOURCES "${LIBRARIES}/SimpleFTPServer/*.cpp")
list(APPEND SRCS ${FTP_SOURCES})
idf_component_register(SRCS ${SRCS}
  INCLUDE_DIRS "${PROJECT}/Sw" "${PRJS_GENERATED_INCLUDE}" "${PRJS}" "${PRJS}/Libs/GnBasicLib" "${PRJS}/Libs/GnAqPrjsLib"
    "${PRJS}/Libs/GnFtpServer" "${LIBRARIES}/ArduinoJson/src" "${LIBRARIES}/SimpleFTPServer"
  REQUIRES arduino esp_wifi esp_event esp_netif app_update nvs_flash bootloader_support spi_flash joltwallet__littlefs)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDUINO_ARCH_ESP32)
