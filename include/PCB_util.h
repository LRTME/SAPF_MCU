/**************************************************************
* FILE:         PCB_util.h 
* DESCRIPTION:  definitions for PCB Initialization & Support Functions
* AUTHOR:       Mitja Nemec
*
**************************************************************/
#ifndef   PCB_UTIL_H
#define   PCB_UTIL_H

#include    "F28x_Project.h"
#include    "define.h"

/**************************************************************
* Funckija ki brcne zunanji WatchDog (WD_KICK) se lahko
* klice samo iz periodicne prekinitve
**************************************************************/
extern void PCB_WD_KICK_int(void);
/**************************************************************
* Funkcije za branje signalov s CPLD
***************************************************************
* Funckija, ki vrne stanje strojne zascite s CPLD (trip)
**************************************************************/
extern bool PCB_CPLD_trip(void);
/**************************************************************
* Funckija stanja "over_voltage" z CPLD (over_voltage)
**************************************************************/
extern bool PCB_CPLD_over_voltage(void);
/**************************************************************
* Funckija stanja "over_current_supply" s CPLD
* (over_current_supply)
**************************************************************/
extern bool PCB_CPLD_over_current_supply(void);
/**************************************************************
* Funckija stanja "over_current_filter" s CPLD
* (over_current_filter)
**************************************************************/
extern bool PCB_CPLD_over_current_filter(void);
/**************************************************************
* Funkcije izhodov na CPLD
***************************************************************
* Funckija izhoda MOSFET_MCU
**************************************************************/
extern void PCB_CPLD_MOSFET_MCU_on(void);
extern void PCB_CPLD_MOSFET_MCU_off(void);
bool PCB_CPLD_MOSFET_MCU_status(void);
/**************************************************************
* Funckija izhoda LATCH_RESET
**************************************************************/
extern void PCB_CPLD_LATCH_RESET(void);
/**************************************************************
* Funkcije za vklop/izklop relejev (preko CPLD)
***************************************************************
* Funckija, ki vklopi Rele1 (Supply_main_relay)
**************************************************************/
extern void PCB_relay1_on(void);
/**************************************************************
* Funckija, ki izklopi Rele1 (Supply_main_relay)
**************************************************************/
extern void PCB_relay1_off(void);
/**************************************************************
* Funckija, ki vklopi Rele2 (Supply_resistor_relay)
**************************************************************/
extern void PCB_relay2_on(void);
/**************************************************************
* Funckija, ki izklopi Rele2 (Supply_resistor_relay)
**************************************************************/
extern void PCB_relay2_off(void);
/**************************************************************
* Funckija, ki vklopi Rele3 (Filter_main_relay)
**************************************************************/
extern void PCB_relay3_on(void);
// relay 3 status
bool PCB_relay3_status(void);
/**************************************************************
* Funckija, ki izklopi Rele3 (Filter_main_relay)
**************************************************************/
extern void PCB_relay3_off(void);
/**************************************************************
* Funkcija vklopa/izklopa 5V_ISO linije
**************************************************************/
extern void PCB_5V_ISO_on(void);
extern void PCB_5V_ISO_off(void);
/**************************************************************
* Funkcije za vklop/izklop LED
***************************************************************
* Funckija, ki vklopi LED FAULT (SIG_FAULT)
**************************************************************/
extern void PCB_LED_FAULT_on(void);
/**************************************************************
* Funckija, ki izklopi LED FAULT (SIG_FAULT)
**************************************************************/
extern void PCB_LED_FAULT_off(void);
/**************************************************************
* Funckija, ki vklopi LED READY (SIG_READY)
**************************************************************/
extern void PCB_LED_READY_on(void);
/**************************************************************
* Funckija, ki izklopi LED READY (SIG_READY)
**************************************************************/
extern void PCB_LED_READY_off(void);
/**************************************************************
* Funckija, ki vklopi LED WORKING (SIG_WORKING)
**************************************************************/
extern void PCB_LED_WORKING_on(void);
/**************************************************************
* Funckija, ki izklopi LED WORKING (SIG_WORKING)
**************************************************************/
extern void PCB_LED_WORKING_off(void);

/**************************************************************
* Funckija, ki togla LED READY (SIG_READY)
**************************************************************/
extern void PCB_LED_READY_toggle(void);

/**************************************************************
* Funkcija, ki toggla LED WORKING (SIG_WORKING)
**************************************************************/
extern void PCB_LED_WORKING_toggle(void);

/**************************************************************
* Funkcije za stanje tipk
***************************************************************
* Funckija ki vrne stanje ENABLE tipke (SW_ENABLE)
**************************************************************/
extern bool PCB_SW_ENABLE(void);
/**************************************************************
* Funckija ki vrne stanje RESET tipke (SW_RESET)
**************************************************************/
extern bool PCB_SW_RESET(void);
/**************************************************************
* Funkcije ki manipulirajo LED diodo  na ControlCardu
**************************************************************/
extern void PCB_LEDcard_on(void);
extern void PCB_LEDcard_off(void);
extern void PCB_LEDcard_toggle(void);
/**************************************************************
* Funckija ki vrne stanje strojne zascite
**************************************************************/
//extern bool PCB_HW_trip(void);
/**************************************************************
* Funckija ki inicializira tiskanino
**************************************************************/
extern void PCB_init(void);

#endif  // end of PCB_UTIL_H definition

