#ifndef IOTNET_FIRMWARE_FLASHER_H
#define IOTNET_FIRMWARE_FLASHER_H

namespace iotnetesp32::ota {

class FirmwareFlasher {
  public:
    static bool downloadAndFlash(const char *url);
};

}

#endif
