#
# Copyright 2013-2022 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

if(RF_FOUND)

  # Include common RF files
  set(SOURCES_RF "")
  list(APPEND SOURCES_RF rf_imp.c)

  # List of dynamic RF plugins
  set(DYNAMIC_PLUGINS "")
  add_definitions(-DENABLE_RF_PLUGINS)

  if(IIO_FOUND)
    add_definitions(-DENABLE_IIO)
    set(SOURCES_IIO rf_iio_imp.c)
    add_library(srsran_rf_iio SHARED ${SOURCES_IIO})
    set_target_properties(srsran_rf_iio PROPERTIES VERSION ${SRSRAN_VERSION_STRING} SOVERSION ${SRSRAN_SOVERSION})
    list(APPEND DYNAMIC_PLUGINS srsran_rf_iio)

    target_link_libraries(srsran_rf_iio srsran_phy ${LIBIIO_LIBRARIES} ${LIBAD9361_LIBRARIES})
    install(TARGETS srsran_rf_iio DESTINATION ${LIBRARY_DIR})
  endif(IIO_FOUND)

  if(RFDC_FOUND)
    add_definitions(-DENABLE_RFDC -DXPS_BOARD_ZCU111)
    set(SOURCES_RFDC xrfdc/rf_xlnx_rfdc_imp.c xrfdc/xrfdc_clk.c)
    add_library(srsran_rf_rfdc SHARED ${SOURCES_RFDC})
    set_target_properties(srsran_rf_rfdc PROPERTIES VERSION ${SRSRAN_VERSION_STRING} SOVERSION ${SRSRAN_SOVERSION})
    list(APPEND DYNAMIC_PLUGINS srsran_rf_rfdc)

    target_link_libraries(srsran_rf_rfdc srsran_phy ${RFDC_LIBRARY} ${LIBMETAL_LIB})
    install(TARGETS srsran_rf_rfdc DESTINATION ${LIBRARY_DIR})
  endif(RFDC_FOUND)

  # Top-level RF library
  add_library(srsran_rf_object OBJECT ${SOURCES_RF})
  set_property(TARGET srsran_rf_object PROPERTY POSITION_INDEPENDENT_CODE 1)
  add_dependencies(srsran_rf_object ${DYNAMIC_PLUGINS})

  add_library(srsran_rf SHARED $<TARGET_OBJECTS:srsran_rf_object>)
  target_link_libraries(srsran_rf dl)

  target_link_libraries(srsran_rf srsran_phy)
  set_target_properties(srsran_rf PROPERTIES VERSION ${SRSRAN_VERSION_STRING} SOVERSION ${SRSRAN_SOVERSION})
  install(TARGETS srsran_rf DESTINATION ${LIBRARY_DIR})

  message(STATUS "RF plugins to build: ${DYNAMIC_PLUGINS}")

endif(RF_FOUND)
