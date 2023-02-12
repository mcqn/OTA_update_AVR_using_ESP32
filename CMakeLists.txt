
set(LIBRARY_SRCS
  components/logger/logger.c
  components/avr_flash/avr_flash.c
  components/avr_pro_mode/avr_pro_mode.c
  #components/protocol_examples_common/stdin_out.c
  #components/protocol_examples_common/connect.c
  components/hex_parser/hex_parser.c
  )

set(includedirs
  components/logger/include
  components/avr_flash/include
  components/avr_pro_mode/include
  components/protocol_examples_common/include
  components/hex_parser/include

  #../arduino/cores/esp32
  #../arduino/variants/esp32s2
  )

set(srcs ${LIBRARY_SRCS})
#set(requires spi_flash mbedtls mdns esp_adc_cal wifi_provisioning nghttp wpa_supplicant)
set(requires spiffs esp_http_server nvs_flash log)
#set(priv_requires fatfs nvs_flash app_update spiffs bootloader_support openssl bt esp_ipc esp_hid)

idf_component_register(INCLUDE_DIRS ${includedirs} SRCS ${srcs} REQUIRES ${requires}) # PRIV_REQUIRES ${priv_requires})

#target_compile_options(${COMPONENT_TARGET} PUBLIC
#    -DARDUINO=10812
#    -DARDUINO_${idf_target_for_macro}_DEV
#    -DARDUINO_ARCH_ESP32
#    -DARDUINO_BOARD="${idf_target_caps}_DEV"
#    -DARDUINO_VARIANT="${CONFIG_ARDUINO_VARIANT}"
#    -DESP32)

#if(IDF_TARGET MATCHES "esp32s2|esp32s3" AND CONFIG_TINYUSB_ENABLED)
#    maybe_add_component(arduino_tinyusb)
#endif()
