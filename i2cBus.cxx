#pragma once

#include "helpers.cxx"

// The I2C transport, kept apart from the one device that currently uses it because the five
// SDP810s and the PCA9548A that multiplexes them will use the same three calls and no others.
//
// Every transfer goes through the I2C_RDWR ioctl rather than through I2C_SLAVE plus read()/
// write(). Two reasons, and the second is the one that matters later:
//
//   1. I2C_SLAVE fails outright if an in-kernel driver has claimed the address, and nothing here
//      needs to hold an address between calls.
//   2. A register read is a write of the register number followed by a read, and the two must not
//      be separated. As one ioctl the kernel issues a repeated START and holds the bus across it;
//      as two syscalls another thread could address a different device in between. Nothing does
//      that today - the BME280 worker owns its fd - but the mux worker will be switching channels
//      on this same bus, and a transfer that can be split is a fault that appears only under
//      that load.
//
// A failure is reported to the caller and never fatal: a logger that dies because a sensor
// stopped answering loses the channels that were still working.
//
// Every transfer is retried, because on this board it has to be: on some boots - it is bimodal
// per boot - the first transfer after an idle bus is refused and the second one succeeds. That
// measurement, and why the
// retry is counted rather than swallowed, is on I2C_TRANSFER_MAX_ATTEMPTS in dataContracts.hpp.

gint openI2cBus(gint busNumber) {
    auto path = std::format(I2C_BUS_PATH_FORMAT, busNumber);

    auto fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        g_warning("I2C: cannot open '%s': %s. dtparam=i2c_arm registers the adapter but only the"
            " i2c-dev module creates the device node.", path.c_str(), strerror(errno));
        return -1;
    }

    g_message("I2C: bus open at '%s'", path.c_str());
    return fd;
}

gboolean transferI2c(gint fd, guint8 address, struct i2c_msg* messages, guint count) {
    if (fd < 0) { return FALSE; }

    struct i2c_rdwr_ioctl_data transfer = { .msgs = messages, .nmsgs = count };

    for (auto attempt = 1; attempt <= I2C_TRANSFER_MAX_ATTEMPTS; attempt++) {
        // The kernel copies the message array in, so the same one is safe to submit again.
        if (ioctl(fd, I2C_RDWR, &transfer) >= 0) {
            if (attempt > 1) { appData.i2c.recoveredTransfers++; }
            return TRUE;
        }

        if (attempt == 1) { appData.i2c.firstAttemptFailures++; }
        g_usleep(I2C_TRANSFER_RETRY_US);
    }

    // Only an exhausted transfer is worth a line. Warning on the first attempt would log once
    // per sample cycle for a bus that is behaving exactly as measured, which is the mistake the
    // 1-Wire worker already made once - 863 identical warnings in 14 minutes buried the two
    // messages that run existed to produce.
    appData.i2c.exhaustedTransfers++;
    if (appConfig.verboseMode) {
        g_warning("I2C: transfer to 0x%02X failed %d times: %s",
            address, I2C_TRANSFER_MAX_ATTEMPTS, strerror(errno));
    }

    return FALSE;
}

gboolean readI2cRegisters(gint fd, guint8 address, guint8 firstRegister, guint8* out, guint count) {
    struct i2c_msg messages[] = {
        { .addr = address, .flags = 0, .len = 1, .buf = &firstRegister },
        { .addr = address, .flags = I2C_M_RD, .len = (guint16)count, .buf = out },
    };

    return transferI2c(fd, address, messages, getLength(messages));
}

gboolean writeI2cRegister(gint fd, guint8 address, guint8 registerNumber, guint8 value) {
    guint8 payload[] = { registerNumber, value };

    struct i2c_msg messages[] = {
        { .addr = address, .flags = 0, .len = getLength(payload), .buf = payload },
    };

    return transferI2c(fd, address, messages, getLength(messages));
}
