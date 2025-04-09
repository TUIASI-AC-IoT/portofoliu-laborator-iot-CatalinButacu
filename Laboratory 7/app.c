/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_common.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "app.h"
#include "app_log.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// UUID pattern to look for in advertisements
static uint8_t uuid[] = {0xaa, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee};

// Maximum number of devices to track
#define MAX_DEVICES 10

// Structure to store device information
typedef struct {
    uint8_t uuid[16];
    char name[10];
    int8_t rssi;
} device_info_t;

// Array to store discovered devices
static device_info_t discovered_devices[MAX_DEVICES];
static uint8_t device_count = 0;

// Application Init.
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  
  // Initialize the device count
  device_count = 0;
  
  // Initialize the discovered devices array
  memset(discovered_devices, 0, sizeof(discovered_devices));
  
  app_log("Bluetooth Scanner Initialized\n");
}

// Application Process Action.
SL_WEAK void app_process_action(void)
{
  if (app_is_process_required()) {
    /////////////////////////////////////////////////////////////////////////////
    // Put your additional application code here!                              //
    // This is will run each time app_proceed() is called.                     //
    // Do not call blocking functions from here!                               //
    /////////////////////////////////////////////////////////////////////////////
  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  uint8_t *p, *pend;
  uint8_t ad_len, ad_type;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      app_log("Initializing\r\n");
      
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);

      // Start scanning in observation mode
      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                               sl_bt_scanner_discover_observation);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new advertisement packet was received.
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      p = evt->data.evt_scanner_legacy_advertisement_report.data.data;
      pend = p + evt->data.evt_scanner_legacy_advertisement_report.data.len;
      
      while (p < pend) {
        ad_len = *p;
        if (!ad_len) {
          break;
        }
        if (p + ad_len > pend) {
          break;
        }
        ad_type = *(p + 1);
        
        // Check for manufacturer specific data with our UUID
        if (ad_len == 26 && ad_type == 0xFF) {
          uint16_t c_id = *((uint16_t*) (p + 2));
          if (c_id == 0x004C && (!memcmp(p + 6, uuid, sizeof(uuid)/sizeof(uint8_t)))) {
            app_log_hexdump_info(p + 2, 25);
            app_log("\r\n");
          }
        }
        
        // Check for device name
        if (ad_type == 0x09) { // Complete Local Name
          uint8_t name_len = ad_len - 1;
          if (name_len > 9) name_len = 9;
          char device_name[10] = {0};
          memcpy(device_name, p + 2, name_len);
          
          // Store device information if we have space
          if (device_count < MAX_DEVICES) {
            memcpy(discovered_devices[device_count].uuid, uuid, 16);
            strncpy(discovered_devices[device_count].name, device_name, 9);
            discovered_devices[device_count].rssi = evt->data.evt_scanner_legacy_advertisement_report.rssi;
            device_count++;
            
            app_log("Device found: %s, RSSI: %d\n", device_name, 
                   evt->data.evt_scanner_legacy_advertisement_report.rssi);
          }
        }
        
        p += ad_len + 1;
      }
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
