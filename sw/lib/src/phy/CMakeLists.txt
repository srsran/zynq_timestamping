#
# Copyright 2013-2022 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

add_subdirectory(rf)
add_subdirectory(utils)
add_subdirectory(common)

set(srsran_srcs $<TARGET_OBJECTS:srsran_utils> $<TARGET_OBJECTS:srsran_phy_common>)

add_library(srsran_phy STATIC ${srsran_srcs} )
target_link_libraries(srsran_phy pthread m)
