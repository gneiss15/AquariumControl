# CMakeLists.txt, Version: 1.00
idf_component_register(SRCS "main.cpp"
  INCLUDE_DIRS "${MINI_HARDWARE_DIR}"
  REQUIRES driver)
