// ============================================================================
// Copyright (c) 2026 Andrew Young
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
// 
// ============================================================================


#pragma once

#include <asio.hpp>
#include <thread>
#include <string>
#include <atomic>
#include "w65c51.h"

// ============================================================================
// Emulates U8 (MAX232) and J1 (DB9 Connector)
// Bridges the emulated W65C51 ACIA to the host machine's physical COM port.
// ============================================================================
class Serial_Port {
    public:
        Serial_Port(w65c51* acia);
        ~Serial_Port();

        // Open the physical COM Port (e.g., "COM3" or "/dev/ttyUSB0")
        bool connect(const std::string& port_name, unsigned int baud_rate = 9600);
        void disconnect();
        bool is_connected() const { return m_connected; }

        // Update once per frame during the main loop to drain the ACIA's Tx Buffer
        void update();

        // For the UI, adding Sniffer Methods
        size_t get_tx_bytes() const { return m_tx_bytes; }
        size_t get_rx_bytes() const { return m_rx_bytes; }
        const std::string& get_terminal_log() const { return m_terminal_log; }
        void clear_terminal() {
            m_terminal_log.clear();
            m_tx_bytes = 0;
            m_rx_bytes = 0;
        }

    private:
        void start_async_read();

        w65c51* m_acia; // pointer to the W65C51N (U7)

        asio::io_context m_io_context;
        asio::serial_port m_serial;
        std::thread m_io_thread;

        std::atomic<bool> m_connected{false};
        u8 m_rx_buffer; // this is a 1-byte buffer for incoming RS232 data

        size_t m_tx_bytes = 0;
        size_t m_rx_bytes = 0;
        std::string m_terminal_log;

        void log_char(u8 c);
};