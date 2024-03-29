/*
 * Copyright 2016-2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */

/** @file
*
* Bluetooth LE Vendor Specific Client Device
*
* The Hello Client application is designed to connect and access services
* of the Hello Sensor device.  Because handles of the all attributes of
* the Hello Sensor are well known, Hello Client does not perform GATT
* discovery, but uses them directly.  In addition to that Hello Client
* allows another central to connect, so the device will behave as a peripheral
* in one Bluetooth piconet and a central in another.  To accomplish that
* application can do both advertisements and scans.  Hello Client assumes
* that Hello Sensor advertises a known Vendor Specific UUID and connects to
* the device which publishes it.
*
* Features demonstrated
*  - Registration with LE stack for various events
*  - Connection to a central and a peripheral
*  - As a central processing notifications from the server and
*    sending notifications to the client
*  - As a peripheral processing writes from the client and sending writes
*    to the server
*
* To demonstrate the app, work through the following steps.
* 1. Plug the WICED eval board into your computer
* 2. Build and download the application (to the WICED board)
* 3. Connect from some client application (for example LightBlue on iOS)
* 4. From the client application register for notifications
* 5. Make sure that your peripheral device (hello_sensor) is up and advertising
* 6. Push the user button on the board for 6 seconds.  That will start
*    connection process.
* 7. Push the user button on the hello_sensor to deliver notification through
*    hello_client device up to the client.
*
*/
#include "spar_utils.h"
#include "bleprofile.h"
#include "blecen.h"
#include "blecli.h"
#include "bleapp.h"
#include "gpiodriver.h"
#include "string.h"
#include "stdio.h"
#include "platform.h"
#include "blecm.h"
#include "hello_client.h"
#include "hello_sensor.h"
#include "devicelpm.h"
#include "lel2cap.h"
#include "sparcommon.h"

const UINT8 hello_service[16] = {UUID_HELLO_SERVICE};

/******************************************************
 *                      Constants
 ******************************************************/
#define NVRAM_ID_HOST_LIST              0x10    // ID of the memory block used for NVRAM access

#define CONNECT_ANY                     0x01
#define CONNECT_HELLO_SENSOR            0x02
#define SMP_PAIRING                     0x04
#define SMP_ERASE_KEY                   0x08

#define HELLO_CLIENT_MAX_PERIPHERALS    4

#define RMULP_CONN_HANDLE_START         0x40
#define CENTRAL_ROLE                    0
#define PERIPHERAL_ROLE                 1

/******************************************************
 *                     Structures
 ******************************************************/
#pragma pack(1)
//host information for NVRAM
typedef PACKED struct
{
    // BD address of the paired host
    BD_ADDR  bdaddr;

    // Current value of the client configuration descriptor
    UINT16  characteristic_client_configuration;

}  HOSTINFO;
#pragma pack()

/******************************************************
 *               Function Prototypes
 ******************************************************/

static void   hello_client_create(void);
static void   hello_client_timeout(UINT32 count);
static void   hello_client_fine_timeout(UINT32 finecount);
static void   hello_client_app_timer(UINT32 count);
static void   hello_client_app_fine_timer(UINT32 finecount);
static void   hello_client_advertisement_report(HCIULP_ADV_PACKET_REPORT_WDATA *evt);
static void   hello_client_connection_up(void);
static void   hello_client_connection_down(void);
static void   hello_client_smp_pair_result(LESMP_PARING_RESULT result);
static void   hello_client_encryption_changed(HCI_EVT_HDR *evt);
static void   hello_client_notification_handler(int len, int attr_len, UINT8 *data);
static void   hello_client_indication_handler(int len, int attr_len, UINT8 *data);
static void   hello_client_process_rsp(int len, int attr_len, UINT8 *data);
static void   hello_client_process_write_rsp();
static int    hello_client_write_handler(LEGATTDB_ENTRY_HDR *p);
static UINT32 hello_client_interrupt_handler(UINT32 value);
static void   hello_client_timer_callback(UINT32 arg);

/******************************************************
 *               Variables Definitions
 ******************************************************/
/*
 * This is the GATT database for the Hello Client application.  Hello Client
 * can connect to hello sensor, but it also provides service for
 * somebody to access.  The database defines services, characteristics and
 * descriptors supported by the application.  Each attribute in the database
 * has a handle, (characteristic has two, one for characteristic itself,
 * another for the value).  The handles are used by the peer to access
 * attributes, and can be used locally by application, for example to retrieve
 * data written by the peer.  Definition of characteristics and descriptors
 * has GATT Properties (read, write, notify...) but also has permissions which
 * identify if peer application is allowed to read or write into it.
 * Handles do not need to be sequential, but need to be in order.
 */
const UINT8 hello_client_gatt_database[]=
{
    // Handle 0x01: GATT service
    // Service change characteristic is optional and is not present
    PRIMARY_SERVICE_UUID16 (0x0001, UUID_SERVICE_GATT),

    // Handle 0x14: GAP service
    // Device Name and Appearance are mandatory characteristics.  Peripheral
    // Privacy Flag only required if privacy feature is supported.  Reconnection
    // Address is optional and only when privacy feature is supported.
    // Peripheral Preferred Connection Parameters characteristic is optional
    // and not present.
    PRIMARY_SERVICE_UUID16 (0x0014, UUID_SERVICE_GAP),

    // Handle 0x15: characteristic Device Name, handle 0x16 characteristic value.
    // Any 16 byte string can be used to identify the sensor.  Just need to
    // replace the "Hello Client" string below.
    CHARACTERISTIC_UUID16 (0x0015, 0x0016, UUID_CHARACTERISTIC_DEVICE_NAME,
                           LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 16),
       'H','e','l','l','o',' ','C','l','i','e','n','t',0x00,0x00,0x00,0x00,

    // Handle 0x17: characteristic Appearance, handle 0x18 characteristic value.
    // List of approved appearances is available at bluetooth.org.  Current
    // value is set to 0x200 - Generic Tag
    CHARACTERISTIC_UUID16 (0x0017, 0x0018, UUID_CHARACTERISTIC_APPEARANCE,
                           LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 2),
                           BIT16_TO_8(APPEARANCE_GENERIC_TAG),

    // Handle 0x28: Hello Client Service.
    // This is the main proprietary service of Hello Client application.  It has
    // a single characteristic which allows peer to write to and can be configured
    // to send indications to the peer.  Note that UUID of the vendor specific
    // service is 16 bytes, unlike standard Bluetooth UUIDs which are 2 bytes.
    // _UUID128 version of the macro should be used.
    PRIMARY_SERVICE_UUID128 (HANDLE_HELLO_CLIENT_SERVICE_UUID, UUID_HELLO_CLIENT_SERVICE),

    // Handle 0x29: characteristic Hello Notification, handle 0x2a characteristic value
    // we support both notification and indication.  Peer need to allow notifications
    // or indications by writing in the Characteristic Client Configuration Descriptor
    // (see handle 2b below).  Note that UUID of the vendor specific characteristic is
    // 16 bytes, unlike standard Bluetooth UUIDs which are 2 bytes.  _UUID128 version
    // of the macro should be used.
    CHARACTERISTIC_UUID128_WRITABLE (0x0029, HANDLE_HELLO_CLIENT_DATA_VALUE, UUID_HELLO_CLIENT_DATA,
            LEGATTDB_CHAR_PROP_READ | LEGATTDB_CHAR_PROP_WRITE | LEGATTDB_CHAR_PROP_WRITE_NO_RESPONSE | LEGATTDB_CHAR_PROP_NOTIFY | LEGATTDB_CHAR_PROP_INDICATE | LEGATTDB_CHAR_PROP_INDICATE,
            LEGATTDB_PERM_READABLE | LEGATTDB_PERM_AUTH_READABLE | LEGATTDB_PERM_WRITE_CMD  | LEGATTDB_PERM_WRITE_REQ | LEGATTDB_PERM_AUTH_WRITABLE | LEGATTDB_PERM_VARIABLE_LENGTH, 20),
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,

    // Handle 0x2b: Characteristic Client Configuration Descriptor.
    // This is standard GATT characteristic descriptor.  2 byte value 0 means that
    // message to the client is disabled.  Peer can write value 1 or 2 to enable
    // notifications or indications respectively.  Not _WRITABLE in the macro.  This
    // means that attribute can be written by the peer.
    CHAR_DESCRIPTOR_UUID16_WRITABLE (HANDLE_HELLO_CLIENT_CLIENT_CONFIGURATION_DESCRIPTOR,
                                     UUID_DESCRIPTOR_CLIENT_CHARACTERISTIC_CONFIGURATION,
                                     LEGATTDB_PERM_READABLE | LEGATTDB_PERM_AUTH_READABLE | LEGATTDB_PERM_WRITE_REQ | LEGATTDB_PERM_AUTH_WRITABLE, 2),
        0x00,0x00,

    // Handle 0x4d: Device Info service
    // Device Information service helps peer to identify manufacture or vendor
    // of the device.  It is required for some types of the devices (for example HID,
    // and medical, and optional for others.  There are a bunch of characteristics
    // available, out of which Hello Sensor implements 3.
    PRIMARY_SERVICE_UUID16 (0x004d, UUID_SERVICE_DEVICE_INFORMATION),

    // Handle 0x4e: characteristic Manufacturer Name, handle 0x4f characteristic value
    CHARACTERISTIC_UUID16 (0x004e, 0x004f, UUID_CHARACTERISTIC_MANUFACTURER_NAME_STRING, LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 8),
        'I','n','f','i','n','e','o','n',

    // Handle 0x50: characteristic Model Number, handle 0x51 characteristic value
    CHARACTERISTIC_UUID16 (0x0050, 0x0051, UUID_CHARACTERISTIC_MODEL_NUMBER_STRING, LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 8),
        '4','3','2','1',0x00,0x00,0x00,0x00,

    // Handle 0x52: characteristic System ID, handle 0x53 characteristic value
    CHARACTERISTIC_UUID16 (0x0052, 0x0053, UUID_CHARACTERISTIC_SYSTEM_ID, LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 8),
        0xef,0x48,0xa2,0x32,0x17,0xc6,0xa6,0xbc,

    // Handle 0x61: Battery service
    // This is an optional service which allows peer to read current battery level.
    PRIMARY_SERVICE_UUID16 (0x0061, UUID_SERVICE_BATTERY),

    // Handle 0x62: characteristic Battery Level, handle 0x63 characteristic value
    CHARACTERISTIC_UUID16 (0x0062, 0x0063, UUID_CHARACTERISTIC_BATTERY_LEVEL,
                           LEGATTDB_CHAR_PROP_READ, LEGATTDB_PERM_READABLE, 1),
        0x64,
};


const BLE_PROFILE_CFG hello_client_cfg =
{
    /*.fine_timer_interval            =*/ 1000, // ms
    /*.default_adv                    =*/ HIGH_UNDIRECTED_DISCOVERABLE,
    /*.button_adv_toggle              =*/ 0,    // pairing button make adv toggle (if 1) or always on (if 0)
    /*.high_undirect_adv_interval     =*/ 32,   // slots
    /*.low_undirect_adv_interval      =*/ 1024, // slots
    /*.high_undirect_adv_duration     =*/ 30,   // seconds
    /*.low_undirect_adv_duration      =*/ 300,  // seconds
    /*.high_direct_adv_interval       =*/ 0,    // seconds
    /*.low_direct_adv_interval        =*/ 0,    // seconds
    /*.high_direct_adv_duration       =*/ 0,    // seconds
    /*.low_direct_adv_duration        =*/ 0,    // seconds
    /*.local_name                     =*/ "Hello Client", // [LOCAL_NAME_LEN_MAX];
    /*.cod                            =*/ {BIT16_TO_8(APPEARANCE_GENERIC_TAG),0x00}, // [COD_LEN];
    /*.ver                            =*/ "1.00",         // [VERSION_LEN];
    /*.encr_required                  =*/ (SECURITY_ENABLED | SECURITY_REQUEST),    // data encrypted and device sends security request on every connection
    /*.disc_required                  =*/ 0,    // if 1, disconnection after confirmation
    /*.test_enable                    =*/ 1,    // TEST MODE is enabled when 1
    /*.tx_power_level                 =*/ 0,    // dbm
    /*.con_idle_timeout               =*/ 0,    // second  0-> no timeout
    /*.powersave_timeout              =*/ 0,    // second  0-> no timeout
    /*.hdl                            =*/ {0x00, 0x00, 0x00, 0x00, 0x00}, // [HANDLE_NUM_MAX];
    /*.serv                           =*/ {0x00, 0x00, 0x00, 0x00, 0x00},
    /*.cha                            =*/ {0x00, 0x00, 0x00, 0x00, 0x00},
    /*.findme_locator_enable          =*/ 0,    // if 1 Find me locator is enable
    /*.findme_alert_level             =*/ 0,    // alert level of find me
    /*.client_grouptype_enable        =*/ 1,    // if 1 grouptype read can be used
    /*.linkloss_button_enable         =*/ 0,    // if 1 linkloss button is enable
    /*.pathloss_check_interval        =*/ 0,    // second
    /*.alert_interval                 =*/ 0,    // interval of alert
    /*.high_alert_num                 =*/ 0,    // number of alert for each interval
    /*.mild_alert_num                 =*/ 0,    // number of alert for each interval
    /*.status_led_enable              =*/ 1,    // if 1 status LED is enable
    /*.status_led_interval            =*/ 0,    // second
    /*.status_led_con_blink           =*/ 0,    // blink num of connection
    /*.status_led_dir_adv_blink       =*/ 0,    // blink num of dir adv
    /*.status_led_un_adv_blink        =*/ 0,    // blink num of undir adv
    /*.led_on_ms                      =*/ 0,    // led blink on duration in ms
    /*.led_off_ms                     =*/ 0,    // led blink off duration in ms
    /*.buz_on_ms                      =*/ 100,  // buzzer on duration in ms
    /*.button_power_timeout           =*/ 0,    // seconds
    /*.button_client_timeout          =*/ 0,    // seconds
    /*.button_discover_timeout        =*/ 0,     // seconds
    /*.button_filter_timeout          =*/ 0,    // seconds
#ifdef BLE_UART_LOOPBACK_TRACE
    /*.button_uart_timeout            =*/ 15,   // seconds
#endif
};

// Following structure defines UART configuration
const BLE_PROFILE_PUART_CFG hello_client_puart_cfg =
{
    /*.baudrate   =*/ 115200,
    /*.txpin      =*/ PUARTENABLE | GPIO_PIN_UART_TX,
    /*.rxpin      =*/ PUARTENABLE | GPIO_PIN_UART_RX,
};

// Following structure defines GPIO configuration used by the application
const BLE_PROFILE_GPIO_CFG hello_client_gpio_cfg =
{
    /*.gpio_pin =*/
    {
        GPIO_PIN_WP,      // This need to be used to enable/disable NVRAM write protect
        GPIO_PIN_BUTTON,  // Button GPIO is configured to trigger either direction of interrupt
        GPIO_PIN_LED,     // LED GPIO, optional to provide visual effects
        GPIO_PIN_BATTERY, // Battery monitoring GPIO. When it is lower than particular level, it will give notification to the application
        GPIO_PIN_BUZZER,  // Buzzer GPIO, optional to provide audio effects
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 // other GPIOs are not used
    },
    /*.gpio_flag =*/
    {
        GPIO_SETTINGS_WP,
        GPIO_SETTINGS_BUTTON | GPIO_BOTHEDGE_INT,
        GPIO_SETTINGS_LED,
        GPIO_SETTINGS_BATTERY,
        GPIO_SETTINGS_BUZZER,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }
};

typedef struct t_APP_STATE
{
    UINT8   app_config;
    UINT32  app_timer_count;
    UINT32  app_fine_timer_count;

    UINT8   handle_to_central;           // handle of the central connection
    UINT8   num_peripherals;            // number of active peripherals

    UINT16  data_handle;                // handle of the sensor's measurement characteristic
    UINT16  config_handle;              // handle of the sensor's configuration characteristic
    UINT16  data_descriptor_handle;     // handle of the measurements client configuration descriptor

    // space to save device info and smp_info to handle multiple connections
    EMCONINFO_DEVINFO dev_info[HELLO_CLIENT_MAX_PERIPHERALS];
    LESMP_INFO        smp_info[HELLO_CLIENT_MAX_PERIPHERALS];

    HOSTINFO hostinfo;                  // NVRAM save area
} tAPP_STATE;

tAPP_STATE hello_client;

BD_ADDR bd_addr_any                             = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
BD_ADDR hello_client_target_addr                = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
UINT8   hello_client_target_addr_type           = 0;


// Following variables are in ROM
extern BLE_CEN_CFG     blecen_cen_cfg;
extern BLEAPP_TIMER_CB blecen_usertimerCb;

/******************************************************
 *               Function Definitions
 ******************************************************/

// Application initialization
APPLICATION_INIT()
{
    bleapp_set_cfg((UINT8 *)hello_client_gatt_database,
                   sizeof(hello_client_gatt_database),
                   (void *)&hello_client_cfg,
                   (void *)&hello_client_puart_cfg,
                   (void *)&hello_client_gpio_cfg,
                   hello_client_create);
    BLE_APP_ENABLE_TRACING_ON_PUART();
}

typedef void *(*LESMPAPI_API)(void *);
extern LESMPAPI_API *lesmpapi_msgHandlerPtr;
void hello_client_l2cap_smp_data_handler(UINT8 *l2capHdr)
{
    // turn on this code to support initiation of the security from the peripheral
#if 0
    if ((lesmp_pinfo->state == 0) && (*(l2capHdr + 4) == LESMP_CODE_SECURITY_REQ))
    {
        ble_trace0("security request");
        lesmpapi_msgHandlerPtr[LESMP_CODE_SECURITY_REQ](l2capHdr + 4);
        return;
    }
#endif
    lesmp_l2capHandler((LEL2CAP_HDR *)l2capHdr);
}
// Create hello sensor
void hello_client_create(void)
{
    ble_trace0("hello_client_create()\n");
    ble_trace0(bleprofile_p_cfg->ver);

    extern UINT32 blecm_configFlag ;
    blecm_configFlag |= BLECM_DBGUART_LOG | BLECM_DBGUART_LOG_L2CAP | BLECM_DBGUART_LOG_SMP;

    // dump the database to debug uart.
    legattdb_dumpDb();

    memset (&hello_client, 0, sizeof (hello_client));

    hello_client.app_config = 0
                            | CONNECT_HELLO_SENSOR
                            | SMP_PAIRING
                            ;

    // Blecen default parameters.  Change if appropriate
    //blecen_cen_cfg.scan_type                = HCIULP_ACTIVE_SCAN;
    //blecen_cen_cfg.scan_adr_type            = HCIULP_PUBLIC_ADDRESS;
    //blecen_cen_cfg.scan_filter_policy       = HCIULP_SCAN_FILTER_POLICY_ACCEPT_LIST_NOT_USED;
    //blecen_cen_cfg.filter_duplicates        = HCIULP_SCAN_DUPLICATE_FILTER_ON;
    //blecen_cen_cfg.init_filter_policy       = HCIULP_INITIATOR_FILTER_POLICY_ACCEPT_LIST_NOT_USED;
    //blecen_cen_cfg.init_addr_type           = HCIULP_PUBLIC_ADDRESS;
    //blecen_cen_cfg.high_scan_interval       = 96;       // slots
    //blecen_cen_cfg.low_scan_interval        = 2048;     // slots
    //blecen_cen_cfg.high_scan_window         = 48;       // slots
    //blecen_cen_cfg.low_scan_window          = 18;       // slots
    //blecen_cen_cfg.high_scan_duration       = 30;       // seconds
    //blecen_cen_cfg.low_scan_duration        = 300;      // seconds
    //blecen_cen_cfg.high_conn_min_interval   = 40;       // frames
    //blecen_cen_cfg.low_conn_min_interval    = 400;      // frames
    //blecen_cen_cfg.high_conn_max_interval   = 56;       // frames
    //blecen_cen_cfg.low_conn_max_interval    = 560;      // frames
    //blecen_cen_cfg.high_conn_latency        = 0;        // number of connection event
    //blecen_cen_cfg.low_conn_latency         = 0;        // number of connection event
    //blecen_cen_cfg.high_supervision_timeout = 10;       // N * 10ms
    //blecen_cen_cfg.low_supervision_timeout  = 100;      // N * 10ms
    //blecen_cen_cfg.conn_min_event_len       = 0;        // slots
    //blecen_cen_cfg.conn_max_event_len       = 0;        // slots

    //change parameter
    blecen_cen_cfg.filter_duplicates        = HCIULP_SCAN_DUPLICATE_FILTER_OFF;
    blecen_usertimerCb                      = hello_client_timer_callback;
    blecen_cen_cfg.high_supervision_timeout = 400;      // N * 10ms
    blecen_cen_cfg.low_supervision_timeout  = 700;      // N * 10ms

    //enable multi connection
    blecm_ConMuxInit(HELLO_CLIENT_MAX_PERIPHERALS);
    blecm_enableConMux();
    blecm_enablescatternet();

    blecen_Create();

    // we will not do scan until user pushes the button for 5 seconds
    blecen_Scan(NO_SCAN);

    bleprofile_Init(bleprofile_p_cfg);
    bleprofile_GPIOInit(bleprofile_gpio_p_cfg);

    // register connection up and connection down handler.
    bleprofile_regAppEvtHandler(BLECM_APP_EVT_LINK_UP, hello_client_connection_up);
    bleprofile_regAppEvtHandler(BLECM_APP_EVT_LINK_DOWN, hello_client_connection_down);

    // register the handler for the CID.
    lel2cap_regConnLessHandler(6, (LEL2CAP_L2CAPHANDLER)hello_client_l2cap_smp_data_handler);

    // handler for Encryption changed.
    blecm_regEncryptionChangedHandler(hello_client_encryption_changed);

    // handler for Pair result
    lesmp_regSMPResultCb((LESMP_SINGLE_PARAM_CB) hello_client_smp_pair_result);

    // setup the pairing parameters.
    lesmp_setPairingParam(
             LESMP_IO_CAP_DISP_NO_IO,           // IOCapability,
             LESMP_OOB_AUTH_DATA_NOT_PRESENT,   // OOBDataFlag,
             LESMP_AUTH_FLAG_BONDING,           // AuthReq,
             LESMP_MAX_KEY_SIZE,                // MaxEncKeySize,
             // InitiatorKeyDistrib,
             LESMP_KEY_DISTRIBUTION_ENC_KEY
             | LESMP_KEY_DISTRIBUTION_ID_KEY
             | LESMP_KEY_DISTRIBUTION_SIGN_KEY,
             // ResponderKeyDistrib
             LESMP_KEY_DISTRIBUTION_ENC_KEY
             | LESMP_KEY_DISTRIBUTION_ID_KEY
             | LESMP_KEY_DISTRIBUTION_SIGN_KEY
    );
    // register to process peripheral advertisements, notifications and indications
    blecm_RegleAdvReportCb((BLECM_FUNC_WITH_PARAM) hello_client_advertisement_report);
    leatt_regNotificationCb((LEATT_TRIPLE_PARAM_CB) hello_client_notification_handler);
    leatt_regIndicationCb((LEATT_TRIPLE_PARAM_CB) hello_client_indication_handler);

    // GATT client callbacks
    leatt_regReadRspCb((LEATT_TRIPLE_PARAM_CB) hello_client_process_rsp);
    leatt_regReadByTypeRspCb((LEATT_TRIPLE_PARAM_CB) hello_client_process_rsp);
    leatt_regReadByGroupTypeRspCb((LEATT_TRIPLE_PARAM_CB) hello_client_process_rsp);
    leatt_regWriteRspCb((LEATT_NO_PARAM_CB) hello_client_process_write_rsp);

    // register to process client writes
    legattdb_regWriteHandleCb((LEGATTDB_WRITE_CB)hello_client_write_handler);

    // process button
    bleprofile_regIntCb(hello_client_interrupt_handler);

    // need to do adverts to enable peripheral connections
    bleprofile_Discoverable(HIGH_UNDIRECTED_DISCOVERABLE, NULL);

    // change timer callback function.  because we are running ROM app, need to
    // stop timer first.
    bleprofile_KillTimer();
    bleprofile_regTimerCb(hello_client_app_fine_timer, hello_client_app_timer);
    bleprofile_StartTimer();
}

// This function will be called on every connection establishmen
void hello_client_connection_up(void)
{
    UINT8 *p_remote_addr     = (UINT8 *)emconninfo_getPeerAddr();
    UINT8 *p_remote_pub_addr = (UINT8 *)emconninfo_getPeerPubAddr();
    UINT16 con_handle        = emconinfo_getConnHandle();
    int cm_index = blecm_FindConMux(con_handle);

    //delete index first
    if (cm_index >= 0)
    {
        blecm_DelConMux(cm_index);
    }

    //find free index
    cm_index = blecm_FindFreeConMux();

    //set information
    if (cm_index < 0)
    {
        ble_trace0("---!!!hello_client_connection_up failed to get mux\n");
        blecm_disconnect(BT_ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST);
        return;
    }

    // copy dev_pinfo
    memcpy((UINT8 *)&hello_client.dev_info[cm_index], (UINT8 *)emconinfo_getPtr(), sizeof(EMCONINFO_DEVINFO));

    // copy smp_pinfo
    memcpy((UINT8 *)&hello_client.smp_info[cm_index], (UINT8 *)lesmp_getPtr(), sizeof(LESMP_INFO));

    blecm_AddConMux(cm_index, con_handle, sizeof (hello_client_gatt_database), (void *)hello_client_gatt_database,
        &hello_client.dev_info[cm_index], &hello_client.smp_info[cm_index]);

    // if we connected as a central configure peripheral to enable notifications
    if (hello_client.dev_info[cm_index].role == CENTRAL_ROLE)
    {
        hello_client.smp_info[cm_index].smpRole = LESMP_ROLE_INITIATOR;

        if (bleprofile_p_cfg->encr_required == 0)
        {
            UINT16 u16 = 1;
            bleprofile_sendWriteReq(HANDLE_HELLO_SENSOR_CLIENT_CONFIGURATION_DESCRIPTOR, (UINT8 *)&u16, 2);
        }
        else
        {
            // following call will start pairing if devices are not paired, or will request
            // encryption if pairing has been established before
            lesmp_setPtr(&hello_client.smp_info[cm_index]);

            lesmp_startPairing(NULL);
            ble_trace0("starting security\n");
        }

        // count number of peripheral connections
        hello_client.num_peripherals++;
    }
    else
    {
        hello_client.smp_info[cm_index].smpRole = LESMP_ROLE_RESPONDERS;

        hello_client.handle_to_central = con_handle;

        // ask central to set preferred connection parameters
        lel2cap_sendConnParamUpdateReq(100, 116, 0, 500);
    }

    ble_trace4("hello_client_connection_up handle:%x peripheral:%d num:%d to_central:%d\n", con_handle,
    hello_client.dev_info[cm_index].role, hello_client.num_peripherals, hello_client.handle_to_central);

    // if we are not connected to all peripherals restart the scan
    if (hello_client.num_peripherals < HELLO_CLIENT_MAX_PERIPHERALS)
    {
        // if we are not connected to the central enable advertisements
        if (!hello_client.handle_to_central)
        {
            ble_trace0("Adv during conn enable\n");
            blecm_setAdvDuringConnEnable(TRUE);
            bleprofile_Discoverable(HIGH_UNDIRECTED_DISCOVERABLE, NULL);
        }
        else
        {
            ble_trace0("Adv during conn disable\n");
            //            blecm_setAdvDuringConnEnable(FALSE);
        }
    }
}

// This function will be called when connection goes down
void hello_client_connection_down(void)
{
    UINT16 con_handle = emconinfo_getConnHandle();
    int cm_index = blecm_FindConMux(con_handle);

    if (cm_index < 0)
    {
        ble_trace0("Can't find such connection\n");
        return;
    }

    if (hello_client.app_config & SMP_ERASE_KEY)
    {
        lesmpkeys_removeAllBondInfo();
        ble_trace0("Pairing Key removed\n");
    }

    ble_trace3("Conn Down handle:%x Peripheral:%d Disc_Reason: %02x\n", con_handle, hello_client.dev_info[cm_index].role, emconinfo_getDiscReason());

    if (hello_client.dev_info[cm_index].role == PERIPHERAL_ROLE)
    {
        hello_client.handle_to_central = 0;

        // restart scan
        blecm_setAdvDuringConnEnable (TRUE);
    }
    else
    {
        blecli_ClientHandleReset();
        blecen_connDown();
    }

    // delete a connection structure
    memset (&hello_client.dev_info[cm_index], 0x00, sizeof(EMCONINFO_DEVINFO));
    memset (&hello_client.smp_info[cm_index], 0x00, sizeof(LESMP_INFO));

    // count number of peripheral connections
    hello_client.num_peripherals--;

    //delete index
    blecm_DelConMux(cm_index);

    // if we are not connected to all peripherals restart the scan
    if (hello_client.num_peripherals < HELLO_CLIENT_MAX_PERIPHERALS)
    {
        blecen_Scan(LOW_SCAN);
    }
}

void hello_client_timeout(UINT32 count)
{
    ble_trace1("hello_client_timeout:%d", count);
}

void hello_client_fine_timeout(UINT32 count)
{
}

void hello_client_app_timer(UINT32 arg)
{
    switch(arg)
    {
        case BLEPROFILE_GENERIC_APP_TIMER:
            {
                hello_client.app_timer_count++;

                hello_client_timeout(hello_client.app_timer_count);
            }
            break;
    }

    blecen_appTimerCb(arg);
}

void hello_client_app_fine_timer(UINT32 arg)
{
    hello_client.app_fine_timer_count++;
    hello_client_fine_timeout(hello_client.app_fine_timer_count);
}


//
// Process SMP pairing result.  If we successfully paired with the
// central device, save its BDADDR in the NVRAM and initialize
// associated data
//
void hello_client_smp_pair_result(LESMP_PARING_RESULT  result)
{
    blecen_smpBondResult(result);

    if(result == LESMP_PAIRING_RESULT_BONDED)
    {
        // if pairing is successful register with the server to receive notification
        UINT16 u16 = 1;
        bleprofile_sendWriteReq(HANDLE_HELLO_SENSOR_CLIENT_CONFIGURATION_DESCRIPTOR, (UINT8 *)&u16, 2);
    }
}

//
// Process notification from the stack that encryption has been set.  If connected
// client is registered for notification or indication, it is a good time to
// send it out
//
void hello_client_encryption_changed(HCI_EVT_HDR *evt)
{
    UINT8 status = *((UINT8 *)(evt + 1));

    ble_trace1("encryption changed: %02x\n", status);

    blecen_encryptionChanged(evt);
}


void hello_client_timer_callback(UINT32 arg)
{
    ble_trace1("hello_client_timer_callback %d\n", arg);

    switch(arg)
    {
    case BLEAPP_APP_TIMER_SCAN:
        blecen_Scan(LOW_SCAN);
        break;

    case BLEAPP_APP_TIMER_CONN:
        if ((blecen_GetConn() == HIGH_CONN) || (blecen_GetConn() == LOW_CONN))
        {
            blecen_Conn(NO_CONN, NULL, 0);
            blecen_Scan(LOW_SCAN);
            bleprofile_Discoverable(HIGH_UNDIRECTED_DISCOVERABLE, NULL);
            ble_trace0("Connection Fail, Restart Scan and Advertisemnts\n");
        }
        break;
    }
}

void hello_client_advertisement_report(HCIULP_ADV_PACKET_REPORT_WDATA *evt)
{
    UINT8 dataLen = (UINT8)(evt->dataLen);

    // The app may crash because watch dog timer is not getting reset due to a lot of events.
    // Reset watch dog timer every time we see an advertisement.
    wdog_restart();

    // make sure that advertisement data is reasonable
    if (dataLen > HCIULP_MAX_DATA_LENGTH)
    {
        return;
    }
    blecen_leAdvReportCb(evt);

#ifdef HELLO_CLIENT_MIN_RSSI
    if (evt->rssi < HELLO_CLIENT_MIN_RSSI)      // filter out adverts with low RSSI
    {
        return;
    }
#endif

    // parse and connection
    if (hello_client.app_config & CONNECT_HELLO_SENSOR)
    {
        BLE_ADV_FIELD *p_field;
        UINT8         *data = (UINT8 *)(evt->data);
        UINT8         *ptr = data;

        UINT8 state = 0;

        while(1)
        {
            UINT16 uuid;
            p_field = (BLE_ADV_FIELD *)ptr;

            if ((p_field->len == 16 + 1) &&
                (p_field->val == ADV_SERVICE_UUID128_COMP) &&
                (memcmp (p_field->data, hello_service, 16) == 0))
            {
                ble_trace0("Found service, no discoverable high conn\n");

                bleprofile_Discoverable(NO_DISCOVERABLE, NULL);

                blecen_Conn(HIGH_CONN, evt->wd_addr, evt->addressType);
                blecen_Scan(NO_SCAN);
                break;
            }

            ptr += (p_field->len + 1);

            if (ptr >= data + dataLen)
            {
                break;
            }
        }
    }
}


void hello_client_process_rsp(int len, int attr_len, UINT8 *data)
{
    ble_trace2("Client rsp len:%d attr_len:%d\n", len, attr_len);
}

void hello_client_process_write_rsp(void)
{
    ble_trace0("Client write rsp\n");
}

void hello_client_process_data_from_peripheral(int len, UINT8 *data)
{
    // if central allows notifications, forward received data
    // Because we will be sending on the different connection, change Set Pointer to the central context
    if ((hello_client.hostinfo.characteristic_client_configuration & CCC_NOTIFICATION) != 0)
    {
        blecm_SetPtrConMux(hello_client.handle_to_central);
        bleprofile_sendNotification(HANDLE_HELLO_CLIENT_DATA_VALUE, data, len < 20 ? len : 20);
    }
    else if ((hello_client.hostinfo.characteristic_client_configuration & CCC_INDICATION) != 0)
    {
        blecm_SetPtrConMux(hello_client.handle_to_central);
        bleprofile_sendIndication(HANDLE_HELLO_CLIENT_DATA_VALUE, data, len < 20 ? len : 20, NULL);
    }
}

void hello_client_notification_handler(int len, int attr_len, UINT8 *data)
{
    ble_trace2("Notification:%02x, %d\n", (UINT16)attr_len, len);
    ble_tracen((char *)data, len);

    hello_client_process_data_from_peripheral(len, data);
}

void hello_client_indication_handler(int len, int attr_len, UINT8 *data)
{
    ble_trace2("Indication:%02x, %d\n", (UINT16)attr_len, len);
    ble_tracen((char *)data, len);

    hello_client_process_data_from_peripheral(len, data);

    bleprofile_sendHandleValueConf();
}

//
// Process write request or command from peer device
//
int hello_client_write_handler(LEGATTDB_ENTRY_HDR *p)
{
    UINT8  writtenbyte;
    UINT16 handle   = legattdb_getHandle(p);
    int    len      = legattdb_getAttrValueLen(p);
    UINT8  *attrPtr = legattdb_getAttrValue(p);

    ble_trace1("hello_client_write_handler: handle %04x\n", handle);

    // By writing into Characteristic Client Configuration descriptor
    // peer can enable or disable notification or indication
    if ((len == 2) && (handle == HANDLE_HELLO_CLIENT_CLIENT_CONFIGURATION_DESCRIPTOR))
    {
        hello_client.hostinfo.characteristic_client_configuration = attrPtr[0] + (attrPtr[1] << 8);
        ble_trace1("hello_client_write_handler: client_configuration %04x\n", hello_client.hostinfo.characteristic_client_configuration);

        // Save update to NVRAM.  Client does not need to set it on every connection.
        writtenbyte = bleprofile_WriteNVRAM(NVRAM_ID_HOST_LIST, sizeof(hello_client.hostinfo), (UINT8 *)&hello_client.hostinfo);
        ble_trace1("hello_client_write_handler: NVRAM write:%04x\n", writtenbyte);
    }
    else if (handle == HANDLE_HELLO_CLIENT_DATA_VALUE)
    {
        ble_tracen((char *)attrPtr, len);
    }
    else
    {
        ble_trace2("hello_sensor_write_handler: bad write len:%d handle:0x%x\n", len, handle);
        return 0x80;
    }

    return 0;
}

UINT32 hello_client_interrupt_handler(UINT32 value)
{
    BLEPROFILE_DB_PDU db_pdu;
    BOOL   button_pushed = value & 0x01;
    static UINT32 button_pushed_time = 0;

    ble_trace3("(INT)But1:%d But2:%d But3:%d\n", value&0x01, (value& 0x02) >> 1, (value & 0x04) >> 2);
    if (button_pushed)
    {
        button_pushed_time = hello_client.app_timer_count;
    }
    else if (button_pushed_time != 0)
    {
        if (hello_client.app_timer_count - button_pushed_time > 5)
        {
            ble_trace0("Stop adverts and start high scan\n");
            bleprofile_Discoverable(NO_DISCOVERABLE, NULL);
            blecen_Scan(HIGH_SCAN);
        }
        else
        {
            if (hello_client.handle_to_central != 0)
            {
                static char *buf = "From Client\n";
                blecm_SetPtrConMux(hello_client.handle_to_central);
                bleprofile_sendNotification(HANDLE_HELLO_CLIENT_DATA_VALUE, (UINT8 *)buf, 12);
            }
        }
    }
    return 0;
}
