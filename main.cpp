/*
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "ble/BLE.h"
#include "ble/services/HealthThermometerService.h"
#include "ble/services/DeviceInformationService.h"

#define MAC_ADDRESS_LEN 6

static HealthThermometerService *thermometerServicePtr;

static const char     DEVICE_NAME[]        = "Beetle-IoT-TS";
static uint8_t        mac_address[MAC_ADDRESS_LEN];
static char           *version;
static const uint16_t uuid16_list[]        = {GattService::UUID_HEALTH_THERMOMETER_SERVICE,
                                              GattService::UUID_DEVICE_INFORMATION_SERVICE};
static volatile bool  trigger              = false;
static float          currentTemperature   = 36.0;

/* Restart Advertising on disconnection*/
void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    BLE::Instance().gap().startAdvertising();
}

void periodicCallback(void)
{
    trigger = true;
}

/*
 * This function is called when the ble initialization process has failed
 */
void onBleInitError(BLE &ble, ble_error_t error)
{
    /* Avoid compiler warnings */
    (void) ble;
    (void) error;
}

/*
 * Callback triggered when the ble initialization process has finished
 */
void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE&        ble   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        /* In case of error, forward the error handling to onBleInitError */
        onBleInitError(ble, error);
        return;
    }

    /* Ensure that it is the default instance of BLE */
    if(ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }

    ble.gap().onDisconnection(disconnectionCallback);

    /* Setup primary service. */
    thermometerServicePtr = new HealthThermometerService(ble, currentTemperature, HealthThermometerService::LOCATION_EAR);

    /* Setup auxiliary services. */
    DeviceInformationService deviceInfo(ble, "ARM-SSG", "BEETLE", "0", "r0", "0", "1");

    /* setup advertising */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::THERMOMETER_EAR);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* 1000ms */
    ble.gap().startAdvertising();
}

/*
 * Display the Beetle information on the serial port
 */
void displayDeviceInfo(void)
{
    /* Get System Version */
    version = (char *)SystemCoreGetVersion();
    /* Get Mac Address */
    __System_Config_GetBDAddr(mac_address, MAC_ADDRESS_LEN);

    /* Print the information */
    printf("%s\n", version);
    printf("Device Name: %s\n", DEVICE_NAME);
    printf("MAC Address: %x:%x:%x:%x:%x:%x\n", mac_address[5],
            mac_address[4],
            mac_address[3],
            mac_address[2],
            mac_address[1],
            mac_address[0]);
    printf("==================\n");
    printf("Welcome to MBED-OS\n");
}

int main(void)
{
    displayDeviceInfo();

    LowPowerTicker ticker;
    ticker.attach(periodicCallback, 1);
    BLE &ble = BLE::Instance();
    ble.init(bleInitComplete);

    /* SpinWait for initialization to complete. This is necessary because the
     * BLE object is used in the main loop below. */
    while (ble.hasInitialized()  == false) { /* spin loop */ }

    while (true) {
        if (trigger && ble.gap().getState().connected) {
            trigger = false;

            /* In our case, we simply update the dummy temperature measurement. */
            currentTemperature += 0.1;
            thermometerServicePtr->updateTemperature(currentTemperature);
        } else {
            ble.waitForEvent();
        }
    }
}
