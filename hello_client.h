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
* Bluetooth LE Vendor Specific Device
*
* This file provides definitions and function prototypes for Hello Client
* device
*
*/
#ifndef HELLO_CLIENT_H
#define HELLO_CLIENT_H

// following definitions for handles used in the GATT database
#define HANDLE_HELLO_CLIENT_SERVICE_UUID                    0x28
#define HANDLE_HELLO_CLIENT_DATA_VALUE                      0x2a
#define HANDLE_HELLO_CLIENT_CLIENT_CONFIGURATION_DESCRIPTOR 0x2b


// Please note that all UUIDs need to be reversed when publishing in the database
// {DC03900D-7C54-44FA-BCA6-C61732A248EF}
// static const GUID UUID_HELLO_CLIENT_SERVICE = { 0xdc03900d, 0x7c54, 0x44fa, { 0xbc, 0xa6, 0xc6, 0x17, 0x32, 0xa2, 0x48, 0xef } };
#define UUID_HELLO_CLIENT_SERVICE           0xef, 0x48, 0xa2, 0x32, 0x17, 0xc6, 0xa6, 0xbc, 0xfa, 0x44, 0x54, 0x7c, 0x0d, 0x90, 0x03, 0xdc

// {B77ACFA5-8F26-4AF6-815B-74D03B4542C5}
// static const GUID UUID_HELLO_CLIENT_DATA = { 0xb77acfa5, 0x8f26, 0x4af6, { 0x81, 0x5b, 0x74, 0xd0, 0x3b, 0x45, 0x42, 0xc5 } };
#define UUID_HELLO_CLIENT_DATA                0xc5, 0x42, 0x45, 0x3b, 0xd0, 0x74, 0x5b, 0x81, 0xf6, 0x4a, 0x26, 0x8f, 0xa5, 0xcf, 0x7a, 0xb7

#endif
