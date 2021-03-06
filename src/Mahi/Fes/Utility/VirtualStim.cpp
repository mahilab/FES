// MIT License
//
// Copyright (c) 2020 Mechatronics and Haptic Interfaces Lab - Rice University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// Author(s): Nathan Dunkelberger (nbd2@rice.edu)

#include <Windows.h>
#include <tchar.h>

#include <Mahi/Fes/Utility/VirtualStim.hpp>
#include <Mahi/Gui.hpp>
#include <Mahi/Util.hpp>
#include <codecvt>
#include <mutex>
#include <thread>

using namespace mahi::util;

namespace mahi {
namespace fes {

VirtualStim::VirtualStim(const std::string& com_port_) :
    Application(500,500,"Virtual Stim"),
    m_com_port(com_port_),
    m_recent_messages(39) {
    open_port();
    configure_port();

    ImGui::StyleColorsLight();
    m_poll_thread = std::thread(&VirtualStim::poll, this);
}

VirtualStim::~VirtualStim() {
    m_poll_thread.join();
}

void VirtualStim::update() {
    ImGui::Begin("Virtual Stimulator Receiver", &m_open);
    {
        ImGui::BeginChild(
            "Child1",
            ImVec2(ImGui::GetWindowContentRegionWidth() * 0.5f, ImGui::GetWindowSize().y - 40),
            true);
        add_monitor(m_recent_message);
        add_monitor(m_channel_setup_message);
        add_monitor(m_scheduler_setup_message);
        add_monitor(m_event_create_message);
        add_monitor(m_scheduler_sync_message);
        add_monitor(m_event_edit_1_message);
        add_monitor(m_event_edit_2_message);
        add_monitor(m_event_edit_3_message);
        add_monitor(m_event_edit_4_message);
        add_monitor(m_scheduler_halt_message);
        add_monitor(m_event_delete_message);
        add_monitor(m_scheduler_delete_message);
        add_monitor(m_unknown_message);
        ImGui::EndChild();
    }

    ImGui::SameLine();
    {
        ImGui::BeginChild("Child2", ImVec2(0, ImGui::GetWindowSize().y - 40), true);
        ImGui::Text("Recent Message Feed");
        ImGui::SameLine();
        ImGui::Checkbox("Pause", &m_pause);
        ImGui::Separator();
        ImGui::Separator();
        for (auto i = 0; i < m_recent_feed.size(); i++) {
            std::vector<std::string> temp_msg_strings = fmt_msg(m_recent_feed[i].message);
            std::string              temp_msg_string  = temp_msg_strings[1];
            char                     time_buff[7];
            sprintf(time_buff, "%4.2f", m_recent_feed[i].time);
            std::string time_string(time_buff);
            ImGui::Text((time_string + ": " + temp_msg_string).c_str());
        }
        ImGui::EndChild();
    }
    // ImGui::EndGroup();
    ImGui::End();

    if (!m_open) {
        quit();
    }
}

std::vector<std::string> VirtualStim::fmt_msg(std::vector<unsigned char> message) {
    std::vector<std::string> formatted_strings;

    std::string temp_msg_unc = "|";
    std::string temp_msg_int = "|";

    for (auto i = 0; i < message.size(); i++) {
        char char_buff[20];
        sprintf(char_buff, "0x%02X", (unsigned int)message[i]);
        temp_msg_unc += char_buff;

        char int_buff[20];
        sprintf(int_buff, "%04i", (unsigned int)message[i]);
        temp_msg_int += int_buff;
        if (i == 3) {
            temp_msg_unc += " | ";
            temp_msg_int += " | ";
        } else if (i != (message.size() - 1)) {
            temp_msg_unc += ", ";
            temp_msg_int += ", ";
        } else {
            temp_msg_unc += "|";
            temp_msg_int += "|";
        }
    }
    formatted_strings.push_back(temp_msg_int);
    formatted_strings.push_back(temp_msg_unc);
    return formatted_strings;
}

void VirtualStim::add_monitor(SerialMessage ser_msg) {
    std::vector<std::string> formatted_strings = fmt_msg(ser_msg.message);

    std::string temp_msg_int = formatted_strings[0];
    std::string temp_msg_unc = formatted_strings[1];

    char time_buff[7];
    sprintf(time_buff, "%4.2f", ser_msg.time);

    ImGui::Separator();
    ImGui::Separator();
    ImGui::Text((ser_msg.message_type + " at " + time_buff + " (msg num " +
                 std::to_string(ser_msg.msg_num) + ")")
                    .c_str());
    ImGui::Separator();
    ImGui::Text(("INT format:" + temp_msg_int).c_str());
    ImGui::Text(("HEX format:" + temp_msg_unc).c_str());
}

bool VirtualStim::open_port() {
    // the comport must be formatted as an LPCWSTR, so we need to get it into that form from a
    // std::string
    std::wstring com_prefix = L"\\\\.\\";
    std::wstring com_suffix =
        std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(m_com_port);
    std::wstring comID = com_prefix + com_suffix;

    // std::wstring stemp = std::wstring(com_port_formatted.begin(), com_port_formatted.end());
    // LPCWSTR com_port_lpcwstr = stemp.c_str();

    m_hComm = CreateFileW(comID.c_str(),                 // port name
                        GENERIC_READ | GENERIC_WRITE,  // Read/Write
                        0,                             // No Sharing
                        NULL,                          // No Security
                        OPEN_EXISTING,                 // Open existing port only
                        0,                             // Non Overlapped I/O
                        NULL);                         // Null for Comm Devices

    // Check if creating the comport was successful or not and log it
    if (m_hComm == INVALID_HANDLE_VALUE) {
        LOG(Error) << "Failed to open Virtual Stimulator";
        return false;
    } else {
        LOG(Info) << "Opened Virtual Stimulator";
    }

    return true;
}

bool VirtualStim::configure_port() {  // configure_port establishes the settings for each serial
                                      // port

    // http://bd.eduweb.hhs.nl/micprg/pdf/serial-win.pdf

    m_dcbSerialParams.DCBlength = sizeof(DCB);

    if (!GetCommState(m_hComm, &m_dcbSerialParams)) {
        LOG(Error) << "Error getting serial port state";
        return false;
    }

    // set parameters to use for serial communication

    // set the baud rate that we will communicate at to 9600
    m_dcbSerialParams.BaudRate = CBR_9600;

    // 8 bits in the bytes transmitted and received.
    m_dcbSerialParams.ByteSize = 8;

    // Specify that we are using one stop bit
    m_dcbSerialParams.StopBits = ONESTOPBIT;

    // Specify that we are using no parity
    m_dcbSerialParams.Parity = NOPARITY;

    // Disable all parameters dealing with flow control
    m_dcbSerialParams.fOutX       = FALSE;
    m_dcbSerialParams.fInX        = FALSE;
    m_dcbSerialParams.fRtsControl = RTS_CONTROL_DISABLE;
    m_dcbSerialParams.fDtrControl = DTR_CONTROL_DISABLE;

    // Set communication parameters for the serial port
    if (!SetCommState(m_hComm, &m_dcbSerialParams)) {
        LOG(Error) << "Error setting serial port state";
        return false;
    }

    COMMTIMEOUTS timeouts                = {0};
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 50;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(m_hComm, &timeouts)) {
        LOG(Error) << "Error setting serial port timeouts";
        return false;
    }
    return true;
}

void VirtualStim::poll() {
    // bool done_reading = false;
    Clock poll_clock;
    poll_clock.restart();
    while (m_open) {
        DWORD         header_size = 4;
        unsigned char msg_header[4];

        DWORD dwBytesRead = 0;

        if (!ReadFile(m_hComm, msg_header, header_size, &dwBytesRead, NULL)) {
            LOG(Error) << "Error reading from comport.";
        }
        if (dwBytesRead != 0) {
            DWORD body_size = (unsigned int)msg_header[3] + 1;
            std::unique_ptr<unsigned char[]> msg_body(new unsigned char[body_size]);
            if (!ReadFile(m_hComm, msg_body.get(), body_size, &dwBytesRead, NULL)) {
                LOG(Error) << "Could not read message body";
            } else {
                if (msg_header[0] == (unsigned char)0x04 && msg_header[1] == (unsigned char)0x80) {
                    m_msg_count += 1;
                    std::vector<unsigned char> msg;
                    for (unsigned int i = 0; i < (header_size + body_size); i++) {
                        if (i < header_size)
                            msg.push_back(msg_header[i]);
                        else
                            msg.push_back(msg_body[i - header_size]);
                    }
                    m_recent_message.message = msg;
                    m_recent_message.time    = poll_clock.get_elapsed_time().as_seconds();
                    m_recent_message.msg_num = m_msg_count;
                    m_recent_messages.push_back(m_recent_message);
                    if (!m_pause)
                        m_recent_feed = m_recent_messages.get_vector();
                    switch (msg_header[2]) {
                        // mel::print("here");
                        case (unsigned char)0x47:
                            m_channel_setup_message.message = msg;
                            m_channel_setup_message.time = poll_clock.get_elapsed_time().as_seconds();
                            m_channel_setup_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x10:
                            m_scheduler_setup_message.message = msg;
                            m_scheduler_setup_message.time = poll_clock.get_elapsed_time().as_seconds();
                            m_scheduler_setup_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x17:
                            m_event_delete_message.message = msg;
                            m_event_delete_message.time = poll_clock.get_elapsed_time().as_seconds();
                            m_event_delete_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x04:
                            m_scheduler_halt_message.message = msg;
                            m_scheduler_halt_message.time =
                                poll_clock.get_elapsed_time().as_seconds();
                            m_scheduler_halt_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x1B:
                            m_scheduler_sync_message.message = msg;
                            m_scheduler_sync_message.time =
                                poll_clock.get_elapsed_time().as_seconds();
                            m_scheduler_sync_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x15:
                            m_event_create_message.message = msg;
                            m_event_create_message.time = poll_clock.get_elapsed_time().as_seconds();
                            m_event_create_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x12:
                            m_scheduler_delete_message.message = msg;
                            m_scheduler_delete_message.time =
                                poll_clock.get_elapsed_time().as_seconds();
                            m_scheduler_delete_message.msg_num = m_msg_count;
                            break;
                        case (unsigned char)0x19:
                            if (msg_body[0] == (unsigned char)0x01) {
                                m_event_edit_1_message.message = msg;
                                m_event_edit_1_message.time =
                                    poll_clock.get_elapsed_time().as_seconds();
                                m_event_edit_1_message.msg_num = m_msg_count;
                            } else if (msg_body[0] == (unsigned char)0x02) {
                                m_event_edit_2_message.message = msg;
                                m_event_edit_2_message.time =
                                    poll_clock.get_elapsed_time().as_seconds();
                                m_event_edit_2_message.msg_num = m_msg_count;
                            } else if (msg_body[0] == (unsigned char)0x03) {
                                m_event_edit_3_message.message = msg;
                                m_event_edit_3_message.time =
                                    poll_clock.get_elapsed_time().as_seconds();
                                m_event_edit_3_message.msg_num = m_msg_count;
                            } else if (msg_body[0] == (unsigned char)0x04) {
                                m_event_edit_4_message.message = msg;
                                m_event_edit_4_message.time =
                                    poll_clock.get_elapsed_time().as_seconds();
                                m_event_edit_4_message.msg_num = m_msg_count;
                            }
                            break;
                        default:
                            m_unknown_message.message = msg;
                            m_unknown_message.time    = poll_clock.get_elapsed_time().as_seconds();
                            m_unknown_message.msg_num = m_msg_count;
                            break;
                    }
                }
            }
        }
    }
}
}  // namespace fes
}  // namespace mahi