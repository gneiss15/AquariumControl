# CMakeLists.txt, Version: 1.03
set(PRJS "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(PROJECT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(LIBRARIES "${PRJS_IDF_DEPENDENCIES}/libraries")
set(SRCS main.cpp)
foreach(NAME EspIndexHtmlCheck EspSignedArtifact SwitchTimes TmrSwA8TPowerMeter TmrSwChannels TmrSwConfig TmrSwHw TmrSwStatusApi TmrSwTimeSync TmrSwWebSocket TmrSwWifi)
  list(APPEND SRCS "${PROJECT}/Sw/${NAME}.cpp")
endforeach()
foreach(NAME GnBasics GnTime GnMemory GnCoopTimer GnHeaterControl GnDeviceIdentity GnUdpStatus GnTimeSync GNPlatformESP Esp32ResetReasonName)
  list(APPEND SRCS "${PRJS}/Libs/GnBasicLib/${NAME}.cpp")
endforeach()
list(APPEND SRCS "${PRJS}/Libs/GnFtpServer/MyFtpServer.cpp")
file(GLOB FTP_SOURCES "${LIBRARIES}/SimpleFTPServer/*.cpp")
list(APPEND SRCS ${FTP_SOURCES})
idf_component_register(SRCS ${SRCS}
  INCLUDE_DIRS "${PROJECT}/Sw" "${PRJS_GENERATED_INCLUDE}" "${PRJS}"
    "${PRJS}/Libs/GnBasicLib" "${PRJS}/Libs/GnFtpServer"
    "${LIBRARIES}/ArduinoJson/src" "${LIBRARIES}/SimpleFTPServer"
  REQUIRES esp_driver_pcnt esp_driver_ledc arduino esp_wifi esp_event esp_netif app_update nvs_flash bootloader_support spi_flash espressif__mdns joltwallet__littlefs)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDUINO_ARCH_ESP32)
