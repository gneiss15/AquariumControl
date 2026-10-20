# CMakeLists.txt, Version: 1.05
set(PRJS "${CMAKE_CURRENT_LIST_DIR}/../../../..")
set(GENERATED "${PRJS}/TmrSwShelly/generated/build/rescue")
idf_component_register(SRCS "main.cpp" "ActivationGate.cpp" "RollbackService.cpp" "../Runtime/Migration.c" "${GENERATED}/EspRescueUpdate.cpp"
  "${GENERATED}/EspRescueUpdater.cpp" "${PRJS}/Libs/Common/EspUpdate/EspSignedArtifact.cpp"
  EMBED_FILES "${GENERATED}/NewTable.bin"
  INCLUDE_DIRS "." "../Runtime" ".." "../Runtime" "${GENERATED}"
  "${PRJS}/Libs/Common/EspRescue" "${PRJS}/Libs/Common/EspUpdate"
  REQUIRES esp_wifi app_update nvs_flash driver esp_timer mbedtls bootloader_support spi_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=stringop-overread)
