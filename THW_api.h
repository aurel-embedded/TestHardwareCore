/*
 * THW_api.h
 *
 *  Created on: 24 févr. 2026
 *      Author: AurelienPajadon
 */

#ifndef TESTHARDWARECORE_THW_API_H_
#define TESTHARDWARECORE_THW_API_H_

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <Config/APP.h>
#include <Tools/serial_VT100.h>
#include <thw_status.h>
#include <TestHardwareCore/include/thw_transport_if.h>
#include <TestHardwareCore/interfaces/thw_io_if.h>
#include <TestHardwareCore/interfaces/thw_time_if.h>

//----------------------------------------------------------------------
// MENU
#define THW_MENU_ITEM_NAME_SIZE 50
typedef struct _st_thw_menuItem{
	 char 		name[THW_MENU_ITEM_NAME_SIZE];
	 void		(*pActionFn)(void*);
	 void *		info;
	 bool		skipAutoRefresh; // If true, auto refresh is skipped when this action is executed
}st_thw_menuItem;

// Gestion des menus
typedef struct _st_thw_actualMenu{
    struct{
    	const st_thw_menuItem* 	tab;       				// Pointer to menu entries
		uint16_t 				size;                 	// Number of menu entries
    }						menu;
	void 					(*displayMenu)(void);		// Menu display function
    void 					(*onInit)(void);        	// Called before entering the menu
    void 					(*onExit)(void);        	// Called when leaving the menu
	void 					(*refreshFn)(void);			// Optional periodic refresh
	uint16_t				refreshPeriodInMs;			// Refresh period in ms
}st_thw_menu;

// Commande commune à tous les menus
typedef enum{
	thw_cmdBack = 0,
	thw_menuNumber_InvalidChoice = 0xFFFF
}eHwt_commonCmd;
//----------------------------------------------------------------------

typedef enum{
	thw_testResult_testNA = 0,
	thw_testResult_testRunning,
	thw_testResult_testERROR,
	thw_testResult_testOK,
}thw_testResult_t;

//----------------------------------------------------------------------
extern void THW_displayActionMenu(st_thw_menuItem *pMenuItems, uint8_t menuItemsQty);


thw_status_t 	THW_init(thw_io_if_t* io, thw_time_if_t* time, void (*entryTestFn)(void*));
thw_status_t 	THW_printf(const char *fmt, ...);
void 			THW_process(void);
void 			THW_setMenu(const st_thw_menu* menu);


// Definition de macros de gestion de l'ecran
#define THW_printfCL(...)		THW_printf(__VA_ARGS__ VT100_COLOR_RESET"\r\n")
#define THW_clearScreen()		THW_printf(VT100_CLEARSCR)
#define THW_clearEndOfScreen()	THW_printf(VT100_CLEAREOS)
#define THW_textColor(Color) 	THW_printf(Color)
#define THW_goto(x,y) 			THW_printf(VT100_GOTOYX, x,y)
#define THW_saveCurPos() 		THW_printf(VT100_SAVEPOS)
#define THW_restoreCurPos() 	THW_printf(VT100_RESTOREPOS)
#define THW_xtermTitle(Name) 	THW_printf("\033]0;%s\007", Name)

#define THW_Banner(Menu)	THW_printf(" %s (v%s) | %s\r\n", APP_SoftwareReference, APP_SoftwareVersion, Menu)

#endif /* TESTHARDWARECORE_THW_API_H_ */
