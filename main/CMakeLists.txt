set(srcs
    "main.c"
    "main.cpp"
    "input/Controller.cpp"
    "display/Display.cpp"
    "game/Game.cpp"
    "game/GameTestScreen.cpp"
    "game/MainMenu.cpp"
    "app/GameMode.cpp"
    "assets/TitleBackground.cpp"
    "assets/PlayerSprite.cpp"
    "assets/PlayerPng.cpp"
    "assets/RoomTiles.cpp"
    "world/Room.cpp"
    "timing/FrameTiming.cpp"
    "input/ModeButton.cpp"
    "app/BootMode.cpp"
    "app/ModeSwitch.cpp"
    "storage/SdStorage.cpp"
    "network/WifiConnection.cpp"
    "transfer/Path.cpp"
    "transfer/FileActions.cpp"
    "transfer/FileServer.cpp"
    "transfer/TransferMode.cpp"
    "transfer/TransferView.cpp"
)

set(requires
    "bluepad32"
    "bluepad32_arduino"
    "arduino"
    "btstack"
    "gameduino2"
    "nvs_flash"
    "esp_http_server"
    "esp_wifi"
    "fatfs"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS
        "."
        "input"
        "display"
        "game"
    EMBED_FILES "transfer/web/index.html"
    REQUIRES ${requires}
)
