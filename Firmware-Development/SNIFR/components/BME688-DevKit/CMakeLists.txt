idf_component_register(SRCS "BME688.cpp"
                    INCLUDE_DIRS "." "../../components" "../../appMain/SNIFR"
                    REQUIRES driver
                    PRIV_REQUIRES Bosch-BSEC2-Library-master)
