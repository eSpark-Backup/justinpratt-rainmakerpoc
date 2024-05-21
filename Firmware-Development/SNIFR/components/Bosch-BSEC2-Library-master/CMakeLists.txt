idf_component_register(SRCS "bsec2.cpp" "."
                        INCLUDE_DIRS include
#                        ../Bosch-BME68x-Library-master
                        REQUIRES Bosch-BME68x-Library-master
)

target_link_libraries(${COMPONENT_TARGET} "-L${CMAKE_CURRENT_LIST_DIR}/Precompiled_binaries/esp32")
target_link_libraries(${COMPONENT_TARGET} algobsec)