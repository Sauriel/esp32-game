set(srcs
    "main.c"
    "main.cpp"
    "input/Controller.cpp"
    "display/Display.cpp"
    "game/Game.cpp"
)

set(requires
    "bluepad32"
    "bluepad32_arduino"
    "arduino"
    "btstack"
    "gameduino2"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS
        "."
        "input"
        "display"
        "game"
    REQUIRES ${requires}
)
