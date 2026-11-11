# CMakeLists.txt, Version: 1.00
idf_component_register(
  SRCS "main.cpp" "${RESCUE_SOURCES}/EspRescueUpdate.cpp"
       "${RESCUE_SOURCES}/EspRescueUpdater.cpp" "${RESCUE_SOURCES}/EspSignedArtifact.cpp"
  INCLUDE_DIRS "." "${RESCUE_SOURCES}"
  REQUIRES esp_wifi app_update nvs_flash driver esp_timer mbedtls)
target_compile_definitions(${COMPONENT_LIB} PRIVATE TmrSwA8THardwareSelect=${RESCUE_HARDWARE})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=stringop-overread)
