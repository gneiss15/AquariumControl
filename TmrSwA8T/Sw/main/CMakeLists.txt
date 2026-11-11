# CMakeLists.txt, Version: 1.09
set(PRJS "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(PROJECT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(LIBRARIES "${PRJS_IDF_DEPENDENCIES}/libraries")
set(SRCS main.cpp)
foreach(NAME EspSignedArtifact SwitchTimes TmrSwA8TPowerMeter TmrSwChannels TmrSwConfig TmrSwHw TmrSwStatusApi TmrSwTimeSync TmrSwWebSocket TmrSwWifi)
  list(APPEND SRCS "${PROJECT}/Sw/${NAME}.cpp")
endforeach()
list(APPEND SRCS "${PRJS}/Common/TmrSw/TmrSwCompactStatus.cpp" "${PRJS}/Common/TmrSw/TmrSwRestartLog.cpp")
foreach(NAME GnBasics GnTime GnMemory GnCoopTimer GNPlatformESP Esp32ResetReasonName)
  list(APPEND SRCS "${PRJS}/Libs/GnBasicLib/${NAME}.cpp")
endforeach()
foreach(NAME GnHeaterControl GnDeviceIdentity GnUdpStatus GnUdpStatusWriter GnTimeSync)
  list(APPEND SRCS "${PRJS}/Libs/GnAqPrjsLib/${NAME}.cpp")
endforeach()
list(APPEND SRCS "${PRJS}/Libs/GnFtpServer/MyFtpServer.cpp")
file(GLOB FTP_SOURCES "${LIBRARIES}/SimpleFTPServer/*.cpp")
list(APPEND SRCS ${FTP_SOURCES})
idf_component_register(SRCS ${SRCS}
  INCLUDE_DIRS "${PROJECT}/Sw" "${PRJS_GENERATED_INCLUDE}" "${PRJS}"
    "${PRJS}/Libs/GnBasicLib" "${PRJS}/Libs/GnAqPrjsLib" "${PRJS}/Libs/GnFtpServer"
    "${LIBRARIES}/ArduinoJson/src" "${LIBRARIES}/SimpleFTPServer"
  REQUIRES esp_driver_pcnt esp_driver_ledc arduino esp_wifi esp_event esp_netif app_update nvs_flash bootloader_support spi_flash heap esp_timer joltwallet__littlefs)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDUINO_ARCH_ESP32)
