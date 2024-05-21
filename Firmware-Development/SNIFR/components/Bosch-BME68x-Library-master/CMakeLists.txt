idf_component_register(SRCS "commMux.cpp" "bme68xLibrary.cpp" "src/bme68x.c"
                        INCLUDE_DIRS src "." "src"
                        REQUIRES 
                            driver 
                            esp_timer
)
