# CMakeLists.txt, Version: 1.00
set(Shared "${CMAKE_CURRENT_LIST_DIR}/../../../../Common/EspUpdate")
idf_component_register(
  SRCS "main.cpp" "${Shared}/EspSignedArtifact.cpp"
  INCLUDE_DIRS "${Shared}" "${WRITER_GENERATED}" "../../Rescue/main"
  REQUIRES app_update bootloader_support driver esp_http_server esp_wifi esp_event esp_netif
           mbedtls spi_flash)
