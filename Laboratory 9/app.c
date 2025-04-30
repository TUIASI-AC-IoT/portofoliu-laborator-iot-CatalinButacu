/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "em_device.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "gatt_db.h"
#include "app_log.h"
#include "sl_sleeptimer.h"

// Global variables 
static sl_sleeptimer_timer_handle_t periodic_timer;
static uint32_t timer_interval_ms = 1000;
static uint8_t counter_value = 0;
static uint32_t passkey = 123456;
static bool displaying_passkey = false;
static bool security_enabled = false;

// Function declarations
static void display_passkey(uint32_t passkey);
static void display_connection_status(uint8_t status);
static void periodic_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

// Connection and button state
static volatile uint8_t button_state = 0;
static volatile uint8_t prev_button_state = 0;
static uint8_t connection_handle = 0xFF;
static bool button_io_notification_enabled = false;

// The advertising set handle
static uint8_t advertising_set_handle = 0xff;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  // Enable GPIO peripheral clock
  CMU_ClockEnable(cmuClock_GPIO, true);
  
  // Configure GPIOA 04 as output (LED)
  GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1);
  
  // Configure GPIOC 07 as input (button)
  GPIO_PinModeSet(gpioPortC, 7, gpioModeInputPullFilter, 1);
  
  // Configure interrupt for button on both edges
  GPIO_ExtIntConfig(gpioPortC, 7, 7, true, true, true);
  
  // Enable GPIO interrupts
  NVIC_ClearPendingIRQ(GPIO_ODD_IRQn);
  NVIC_EnableIRQ(GPIO_ODD_IRQn);
  
  // Initialize button state
  button_state = !GPIO_PinInGet(gpioPortC, 7);
  prev_button_state = button_state;

  // Start periodic timer
  sl_status_t sc = sl_sleeptimer_start_periodic_timer(&periodic_timer,
                                                      timer_interval_ms,
                                                      periodic_timer_callback,
                                                      NULL,
                                                      0,
                                                      0);
  app_assert_status(sc);
  app_log("Periodic timer started with %lu ms interval\r\n", timer_interval_ms);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  // Check button state
  uint8_t current_state = !GPIO_PinInGet(gpioPortC, 7);
  if (current_state != prev_button_state) {
    button_state = current_state;
    prev_button_state = current_state;
    sl_bt_external_signal(1);  // Signal the stack to handle button state change
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
  uint8_t recv_val;
  size_t recv_len;

  switch (SL_BT_MSG_ID(evt->header))
  {
    case sl_bt_evt_system_boot_id:
      app_log("Initializing\r\n");
    
      // Configure Bluetooth security
      sc = sl_bt_sm_configure(0, sl_bt_sm_io_capability_displayonly);
      app_assert_status(sc);
      
      // Set passkey to Bluetooth stack
      sc = sl_bt_sm_set_passkey(passkey);
      app_assert_status(sc);
      
      // Set bondable mode
      sc = sl_bt_sm_set_bondable_mode(1); // Bondable = 1
      app_assert_status(sc);
      
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
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
      break;

    // New connection was opened.
    case sl_bt_evt_connection_opened_id:
      connection_handle = evt->data.evt_connection_opened.connection;
      app_log("Connection opened\r\n");
    
      // Increase security immediately after connection
      sc = sl_bt_sm_increase_security(connection_handle);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_passkey_display_id:
      // Display passkey to user
      displaying_passkey = true;
      passkey = evt->data.evt_sm_passkey_display.passkey;
      display_passkey(passkey);
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log("Bonding successful\r\n");
      security_enabled = true;
      display_connection_status(1); // Display success
      displaying_passkey = false;
      break;

    case sl_bt_evt_sm_bonding_failed_id:
      app_log("Bonding failed, reason: 0x%2.2x\r\n", 
              evt->data.evt_sm_bonding_failed.reason);
      security_enabled = false;
      display_connection_status(0); // Display error
      displaying_passkey = false;
      break;

    case sl_bt_evt_connection_closed_id:
      connection_handle = 0xFF;  // Reset connection handle
      button_io_notification_enabled = false;  // Reset notification flag
      
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // Handle notification enable/disable
    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (gattdb_BUTTON_IO == evt->data.evt_gatt_server_characteristic_status.characteristic) {
        if (evt->data.evt_gatt_server_characteristic_status.client_config_flags 
            & sl_bt_gatt_notification) {
          app_log("Button notifications enabled\r\n");
          button_io_notification_enabled = true;
        } else {
          app_log("Button notifications disabled\r\n");
          button_io_notification_enabled = false;
        }
      }
      break;

    // Handle LED characteristic write
    case sl_bt_evt_gatt_server_attribute_value_id:
      if (gattdb_LED_IO == evt->data.evt_gatt_server_attribute_value.attribute) {
        sc = sl_bt_gatt_server_read_attribute_value(gattdb_LED_IO,
                                                   0,
                                                   sizeof(recv_val),
                                                   &recv_len,
                                                   &recv_val);
        app_assert_status(sc);
        
        if (recv_val) {
          GPIO_PinOutSet(gpioPortA, 4);    // Turn LED on
        } else {
          GPIO_PinOutClear(gpioPortA, 4);  // Turn LED off
        }
        app_log("LED= %d\r\n", recv_val);
      }
      break;

    // Handle button state change
    case sl_bt_evt_system_external_signal_id:
      if (connection_handle != 0xFF) {  // If we have an active connection
        // Create a non-volatile copy to fix the warning
        uint8_t non_volatile_state = button_state;
        
        // Update the characteristic value
        sc = sl_bt_gatt_server_write_attribute_value(gattdb_BUTTON_IO,
                                                  0,
                                                  sizeof(non_volatile_state),
                                                  &non_volatile_state);
        app_assert_status(sc);
        
        // Send notification if enabled
        if (button_io_notification_enabled) {
          sc = sl_bt_gatt_server_notify_all(gattdb_BUTTON_IO,
                                          sizeof(non_volatile_state),
                                          &non_volatile_state);
          app_assert_status(sc);
        }
      }
      break;

    default:
      break;
  }
}

/*****************************************************************************/
static void display_passkey(uint32_t key)
{
  app_log("PASSKEY DISPLAY: %06lu\r\n", key);
  
  // Flash LED to indicate passkey is being shown
  for (int i = 0; i < 3; i++) {
    GPIO_PinOutToggle(gpioPortA, 4);
    sl_sleeptimer_delay_millisecond(300);
    GPIO_PinOutToggle(gpioPortA, 4);
    sl_sleeptimer_delay_millisecond(300);
  }
}

/*****************************************************************************/
static void display_connection_status(uint8_t status)
{
  if (status) {
    app_log("PAIRING SUCCESSFUL\r\n");
    // Flash LED quickly 5 times to indicate success
    for (int i = 0; i < 5; i++) {
      GPIO_PinOutToggle(gpioPortA, 4);
      sl_sleeptimer_delay_millisecond(100);
      GPIO_PinOutToggle(gpioPortA, 4);
      sl_sleeptimer_delay_millisecond(100);
    }
  } else {
    app_log("PAIRING FAILED\r\n");
    // Flash LED slowly 3 times to indicate failure
    for (int i = 0; i < 3; i++) {
      GPIO_PinOutToggle(gpioPortA, 4);
      sl_sleeptimer_delay_millisecond(500);
      GPIO_PinOutToggle(gpioPortA, 4);
      sl_sleeptimer_delay_millisecond(500);
    }
  }
}

/*****************************************************************************/
static void periodic_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  
  counter_value++;
  
  if (connection_handle != 0xFF) 
  {  
    app_log("Counter = %d\r\n", counter_value);
  }
}
/*****************************************************************************/
