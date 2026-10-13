#pragma once

namespace Display {
    void begin();
    // Transfer-Aufrufer muss die gemeinsame SPI-Sperre halten.
    // GD2-Typen bleiben privat, insbesondere dessen eigene struct dirent.
    void resumeCommandStream();
    void endCommandStream();
}
