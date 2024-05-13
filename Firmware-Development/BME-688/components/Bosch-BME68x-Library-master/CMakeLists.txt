idf_component_register(SRCS "bme68xLibrary.cpp" "src/bme68x.c"
                        INCLUDE_DIRS src "." "src"
                        REQUIRES 
                            driver 
                            esp_timer
)
