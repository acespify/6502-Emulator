#include "device.h"

device_t::device_t(machine_config &mconfig, const std::string &tag, device_t *owner, u32 clock)
    : m_machine(mconfig),
      m_tag(tag),
      m_owner(owner),
      m_clock(clock)
{
    // In a full MAME build, we would register this device with the machine here.
    // For now, we will simply construct the identity.
    std::cout << "[Device Constructed] " << this->qname()
              << " (" << clock << " Hz)" << std::endl;
}