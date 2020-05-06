/*
 * wui_api.h
 * \brief   interface functions for Web User Interface library
 *
 *  Created on: April 22, 2020
 *      Author: joshy <joshymjose[at]gmail.com>
 */
#include "wui_api.h"
#include "version.h"
#include "otp.h"
#include <string.h>
#include <stdio.h>
#include "ini_handler.h"
#include "eeprom.h"
#include "string.h"
#include <stdbool.h>
#include <time.h>
#include "main.h"
#include "stm32f4xx_hal.h"

#define MAX_UINT16              65535
#define PRINTER_TYPE_ADDR       0x0802002F // 1 B
#define PRINTER_VERSION_ADDR    0x08020030 // 1 B

static bool sntp_time_init = false;

static int ini_handler_func(void *user, const char *section, const char *name, const char *value) {

    ETH_config_t * tmp_config = (ETH_config_t *)user;

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("lan_ip4", "type")) {
        if (strncmp(value, "DHCP", 4) == 0 || strncmp(value, "dhcp", 4) == 0) {
            CHANGE_LAN_TO_DHCP(tmp_config->lan.flag);
            tmp_config->var_mask |= ETHVAR_MSK(ETHVAR_LAN_FLAGS);
        } else if (strncmp(value, "STATIC", 6) == 0 || strncmp(value, "static", 6) == 0) {
            CHANGE_LAN_TO_STATIC(tmp_config->lan.flag);
            tmp_config->var_mask |= ETHVAR_MSK(ETHVAR_LAN_FLAGS);
        }
    } else if (MATCH("lan_ip4", "hostname")) {
        strlcpy(tmp_config->hostname, value, ETH_HOSTNAME_LEN + 1);
        tmp_config->var_mask |= ETHVAR_MSK(ETHVAR_HOSTNAME);
    } else if (MATCH("lan_ip4", "address")) {
        if (ip4addr_aton(value, &tmp_config->lan.addr_ip4)) {
            tmp_config->var_mask |= ETHVAR_MSK(ETHVAR_LAN_ADDR_IP4);
        }
    } else if (MATCH("lan_ip4", "mask")) {
        if (ip4addr_aton(value, &tmp_config->lan.msk_ip4)) {
            tmp_config->var_mask |= ETHVAR_MSK(ETHVAR_LAN_MSK_IP4);
        }
    } else if (MATCH("lan_ip4", "gateway")) {
        if (ip4addr_aton(value, &tmp_config->lan.gw_ip4)) {
            tmp_config->var_mask |= ETHVAR_MSK(ETHVAR_LAN_GW_IP4);
        }
    } else {
        return 0; /* unknown section/name, error */
    }
    return 1;
}

static ini_handler wui_ini_handler = ini_handler_func;

uint32_t load_ini_params(ETH_config_t * config) {
    config->var_mask = ETHVAR_MSK(ETHVAR_LAN_FLAGS);
    load_eth_params(config);
    config->var_mask = 0;

    if(ini_load_file(wui_ini_handler, config)){
        return set_loaded_eth_params(config);
    } else {
        return 0;
    }
}

uint32_t save_eth_params(ETH_config_t * ethconfig) {

    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_FLAGS)) {
        eeprom_set_var(EEVAR_LAN_FLAG, variant8_ui8(ethconfig->lan.flag));
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_ADDR_IP4)) {
        eeprom_set_var(EEVAR_LAN_IP4_ADDR, variant8_ui32(ethconfig->lan.addr_ip4.addr));
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_MSK_IP4)){
        eeprom_set_var(EEVAR_LAN_IP4_MSK, variant8_ui32(ethconfig->lan.msk_ip4.addr));
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_GW_IP4)){
        eeprom_set_var(EEVAR_LAN_IP4_GW, variant8_ui32(ethconfig->lan.gw_ip4.addr));
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_HOSTNAME)) {
        variant8_t hostname = variant8_pchar(ethconfig->hostname, 0, 0);
        eeprom_set_var(EEVAR_LAN_HOSTNAME, hostname);
        //variant8_done() is not called, variant_pchar with init flag 0 doesnt hold its memory
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_TIMEZONE)) {
        eeprom_set_var(EEVAR_TIMEZONE, variant8_i8(ethconfig->timezone));
    }

    return 0;
}

uint32_t load_eth_params(ETH_config_t *ethconfig) {

    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_FLAGS)){
        ethconfig->lan.flag = eeprom_get_var(EEVAR_LAN_FLAG).ui8;
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_ADDR_IP4)) {
        ethconfig->lan.addr_ip4.addr = eeprom_get_var(EEVAR_LAN_IP4_ADDR).ui32;
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_MSK_IP4)) {
        ethconfig->lan.msk_ip4.addr = eeprom_get_var(EEVAR_LAN_IP4_MSK).ui32;
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_LAN_GW_IP4)) {
        ethconfig->lan.gw_ip4.addr = eeprom_get_var(EEVAR_LAN_IP4_GW).ui32;
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_HOSTNAME)) {
        variant8_t hostname = eeprom_get_var(EEVAR_LAN_HOSTNAME);
        strlcpy(ethconfig->hostname, hostname.pch, ETH_HOSTNAME_LEN + 1);
        variant8_done(&hostname);
    }
    if (ethconfig->var_mask & ETHVAR_MSK(ETHVAR_TIMEZONE)) {
        ethconfig->timezone = eeprom_get_var(EEVAR_TIMEZONE).i8;
    }

    return 0;
}

void get_printer_info(printer_info_t *printer_info) {
    // FIRMWARE VERSION
    strlcpy(printer_info->firmware_version, project_version_full, FW_VER_STR_LEN);
    // PRINTER TYPE
    printer_info->printer_type = *(volatile uint8_t *)PRINTER_TYPE_ADDR;
    // PRINTER_VERSION
    printer_info->printer_version = *(volatile uint8_t *)PRINTER_VERSION_ADDR;
    // MAC ADDRESS
    parse_MAC_address(printer_info->mac_address);
    // SERIAL NUMBER
    for (int i = 0; i < OTP_SERIAL_NUMBER_SIZE; i++) {
        printer_info->serial_number[i] = *(volatile char *)(OTP_SERIAL_NUMBER_ADDR + i);
    }
    // UUID - 96 bits
    volatile uint32_t *uuid_ptr = (volatile uint32_t *)OTP_STM32_UUID_ADDR;
    snprintf(printer_info->mcu_uuid, UUID_STR_LEN, "%08lx-%08lx-%08lx", *uuid_ptr, *(uuid_ptr + 1), *(uuid_ptr + 2));
}

void parse_MAC_address(char * dest){
    volatile uint8_t *mac_ptr = (volatile uint8_t *)OTP_MAC_ADDRESS_ADDR;
    snprintf(dest, MAC_ADDR_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
        *mac_ptr, *(mac_ptr + 1), *(mac_ptr + 2), *(mac_ptr + 3), *(mac_ptr + 4), *(mac_ptr + 5));
}

void stringify_eth_for_ini(char * dest, ETH_config_t *config) {
    char addr[IP4_ADDR_STR_SIZE], msk[IP4_ADDR_STR_SIZE], gw[IP4_ADDR_STR_SIZE];

    ip4addr_ntoa_r(&(config->lan.addr_ip4), addr, IP4_ADDR_STR_SIZE);
    ip4addr_ntoa_r(&(config->lan.msk_ip4), msk, IP4_ADDR_STR_SIZE);
    ip4addr_ntoa_r(&(config->lan.gw_ip4), gw, IP4_ADDR_STR_SIZE);

    snprintf(dest, MAX_INI_SIZE,
        "[lan_ip4]\ntype=%s\nhostname=%s\naddress=%s\nmask=%s\ngateway=%s\n",
        IS_LAN_STATIC(config->lan.flag) ? "STATIC" : "DHCP", config->hostname,
        addr, msk, gw);
}

void stringify_eth_for_screen(char * dest, ETH_config_t * config) {
    char addr[IP4_ADDR_STR_SIZE], msk[IP4_ADDR_STR_SIZE], gw[IP4_ADDR_STR_SIZE], mac[MAC_ADDR_STR_LEN];

    ip4addr_ntoa_r(&(config->lan.addr_ip4), addr, IP4_ADDR_STR_SIZE);
    ip4addr_ntoa_r(&(config->lan.msk_ip4), msk, IP4_ADDR_STR_SIZE);
    ip4addr_ntoa_r(&(config->lan.gw_ip4), gw, IP4_ADDR_STR_SIZE);
    parse_MAC_address(mac);

    snprintf(dest, 150, "IPv4 Address:\n  %s      \nIPv4 Netmask:\n  %s      \nIPv4 Gateway:\n  %s      \nMAC Address:\n  %s",
        addr, msk, gw, mac);
}

void update_eth_addrs(ETH_config_t *config){
    config->var_mask = ETHVAR_MSK(ETHVAR_LAN_FLAGS);
    load_eth_params(config);

    if (IS_LAN_STATIC(config->lan.flag)){
        config->var_mask = ETHVAR_STATIC_LAN_ADDRS;
        load_eth_params(config);
    } else {
        get_addrs_from_dhcp(config);
    }
}

uint32_t set_loaded_eth_params(ETH_config_t * config){
    
    
    if (config->var_mask & ETHVAR_MSK(ETHVAR_LAN_FLAGS)) {
        // if lan type is set to STATIC
        if (IS_LAN_STATIC(config->lan.flag)){
            if ((config->var_mask & ETHVAR_STATIC_LAN_ADDRS) != ETHVAR_STATIC_LAN_ADDRS) {
                return 0;
            }
        }
    }
    if (config->var_mask & ETHVAR_MSK(ETHVAR_HOSTNAME)) {
        strlcpy(eth_hostname, config->hostname, ETH_HOSTNAME_LEN + 1);
    }

    // ugly way to aquire lan flags before load
    uint8_t swaper, prev_lan_flag = config->lan.flag;
    uint32_t set_mask = config->var_mask;
    config->var_mask = ETHVAR_MSK(ETHVAR_LAN_FLAGS);
    load_eth_params(config);
    swaper = prev_lan_flag;
    prev_lan_flag = config->lan.flag;
    config->lan.flag = swaper;
    config->var_mask = set_mask;

    if (config->var_mask & ETHVAR_MSK(ETHVAR_LAN_FLAGS)) {
        // if there was a change from STATIC to DHCP
        if (IS_LAN_STATIC(prev_lan_flag) && IS_LAN_DHCP(config->lan.flag)) {
            set_LAN_to_dhcp(config);
        // or STATIC to STATIC
        } else if (IS_LAN_STATIC(config->lan.flag)){
            set_LAN_to_static(config);
        }
        // from DHCP to DHCP: do nothing
    }

    save_eth_params(config);

    return 1;
}

void sntp_get_system_time(char * dest){

  if(sntp_time_init){
    RTC_TimeTypeDef currTime;

    HAL_RTC_GetTime(&hrtc, &currTime, RTC_FORMAT_BIN);

    snprintf(dest, 12, "%02d:%02d:%02d", currTime.Hours, currTime.Minutes, currTime.Seconds);
  } else {
    strcpy(dest, "N/A");
  }
}

void sntp_get_system_date(char * dest){

  if(sntp_time_init){
    RTC_DateTypeDef currDate;

    HAL_RTC_GetDate(&hrtc, &currDate, RTC_FORMAT_BIN);

    snprintf(dest, 13, "%02d.%02d.%d", currDate.Date, currDate.Month + 1, currDate.Year + 1900);
  } else {
    strcpy(dest, "N/A");
  }
}

void sntp_set_system_time(uint32_t sec)
{
  ETH_config_t config;
  config.var_mask = ETHVAR_MSK(ETHVAR_TIMEZONE);
  load_eth_params(&config);

  RTC_TimeTypeDef currTime;
  RTC_DateTypeDef currDate;

  struct tm current_time_val;
  time_t current_time = (time_t)sec + (config.timezone * 3600);

  localtime_r(&current_time, &current_time_val);

  currTime.Seconds = current_time_val.tm_sec;
  currTime.Minutes = current_time_val.tm_min;
  currTime.Hours = current_time_val.tm_hour;
  currDate.Date = current_time_val.tm_mday;
  currDate.Month = current_time_val.tm_mon;
  currDate.Year = current_time_val.tm_year;
  currDate.WeekDay = current_time_val.tm_wday;

  HAL_RTC_SetTime(&hrtc, &currTime, RTC_FORMAT_BIN);
  HAL_RTC_SetDate(&hrtc, &currDate, RTC_FORMAT_BIN);

  sntp_time_init = true;
}