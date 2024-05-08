idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    INCLUDE_DIRS "../components"
                    REQUIRES driver
                    PRIV_REQUIRES Bosch-BSEC2-Library-master)
