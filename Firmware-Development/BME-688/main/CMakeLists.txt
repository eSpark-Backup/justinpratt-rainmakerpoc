idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "." "../components"
                    REQUIRES driver fatfs
                    PRIV_REQUIRES Bosch-BSEC2-Library-master)
