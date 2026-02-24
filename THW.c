/*
 * THW.c
 *
 *  Created on: 24 févr. 2026
 *      Author: AurelienPajadon
 */
#ifdef MODE_THW

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <TestHardwareCore/interfaces/thw_io_if.h>
#include "THW_api.h"
#include <Config/task_config.h>


static struct{
	// Dependency Injection
	thw_io_if_t* io;

	// Refresh Task management
	uint32_t lastRefreshTick;
}thw_ctx;


//-----------------------------------------------------------------------------
// THREAD RX
//-----------------------------------------------------------------------------
osThreadId_t thw_tsk_id;
const osThreadAttr_t thw_tsk_attr = {
		.name = "thw_tsk",
		.stack_size = TSK_CFG__STACK__TSK_THW,
		.priority = TSK_CFG__PRIO__TSK_THW,
};
static void thw_tsk_fn(void *arg);


//------------------------------------------------------------------------------
// MENU
//------------------------------------------------------------------------------
st_thw_actualMenu thw_actualMenu;	// Structure sur le menu actuellement utilisé

static void manageChoice(char code);
static void displayMenuAction(void);

//------------------------------------------------------------------------------
// DEPENDENCY INJECTION
//------------------------------------------------------------------------------
static void (*thw_entryTestFn)(void*) = NULL;
//=============================================================================
//						Thread Functions
//=============================================================================
static void thw_tsk_fn(void *argument)
{
	uint32_t 	userChoice;
	char rxLine[128];

	THW_printf(VT100_NORMAL);

	// Display Entry point
	if (thw_entryTestFn != NULL)
		thw_entryTestFn(NULL);
	else
	{
		THW_printf("[THW] No entry point defined!\r\n");
		return;
	}

	// Automatic Call onInit()
	if (thw_actualMenu.onInit)
		thw_actualMenu.onInit();

	THW_clearScreen();
	if(thw_actualMenu.displayMenu != NULL)
		thw_actualMenu.displayMenu();
	displayMenuAction();

	while(1){
		osDelay(100);

		// Get User Choice
		//----------------------
		if(thw_ctx.io->readLine(rxLine, sizeof(rxLine)))
		{
			// 	Manage user Choice
			//----------------------
			userChoice = atoi(rxLine);

			// Capture current menu reference before executing action
			const void* prevMenuTab  = thw_actualMenu.menu.tab;
			uint16_t    prevMenuSize = thw_actualMenu.menu.size;

			// Execute user choice (may change menu via setActive)
			manageChoice(userChoice);

			// Detect if a new menu has been activated
			bool menuChanged =
					(prevMenuTab  != thw_actualMenu.menu.tab) ||
					(prevMenuSize != thw_actualMenu.menu.size) ||
					(userChoice == 0);

			// Initialize Test
			//----------------------
			if (menuChanged && thw_actualMenu.onInit)
				thw_actualMenu.onInit();

			// 		Display
			//----------------------
			THW_clearScreen();
			if(thw_actualMenu.displayMenu != NULL)
				thw_actualMenu.displayMenu();
			displayMenuAction();
			THW_saveCurPos();
		}

		// Refresh auto if needed
		if(thw_actualMenu.refreshFn != NULL)
		{
		    uint32_t now = osKernelGetTickCount();
		    if(now - thw_ctx.lastRefreshTick >= thw_actualMenu.refreshPeriodInMs)
		    {
		    	thw_ctx.lastRefreshTick = now;
		    	THW_saveCurPos();
		        thw_actualMenu.refreshFn();
				THW_restoreCurPos();
		    }
		}
	}
}

//------------------------------------------------------------------------------
/// \brief Generic choice handler (called by ManageChoice or directly)
///        Uses thw_actualMenu to access menu entries and onExit() if defined.
//------------------------------------------------------------------------------
static void manageChoice(char code)
{
	if (code == 0)
	{
		// Raz Refresh before setting new menu
		thw_actualMenu.refreshFn = NULL;

		// Call menu-specific exit handler if any
		if (thw_actualMenu.onExit){
			// Save onExit pointer
			void (*onExit_save)(void) = thw_actualMenu.onExit;

			// Clear current menu to avoid re-entrance issues
			memset(&thw_actualMenu, 0, sizeof(st_thw_actualMenu));

			// Call onExit
			onExit_save();
		}
		return;
	}

	if (code < 0)
	{
		THW_printf("[Invalid choice]\r\n");
		return;
	}

	// Map visible choice number -> real index in menu.tab[]
	uint16_t visibleIndex = 1;

	for (uint16_t i = 0; i < thw_actualMenu.menu.size; i++)
	{
		const st_thw_menuItem* item = &thw_actualMenu.menu.tab[i];

		if (item->pActionFn == NULL)
			continue; // skip separators

		if (visibleIndex == code)
		{
			item->pActionFn(item->info);
			return;
		}

		visibleIndex++;
	}

	// If we reach here, choice was out of range
	THW_printf("[Invalid choice]\r\n");
	return;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void displayMenuAction(void)
{
	// --- Display generic Content ---
	if (thw_actualMenu.menu.tab != NULL && thw_actualMenu.menu.size > 0)
	{
		uint16_t actionIndex = 1;
		for (uint16_t i = 0; i < thw_actualMenu.menu.size; i++)
		{
			if(thw_actualMenu.menu.tab[i].pActionFn != NULL){
				THW_printf("%2d - %s\r\n", actionIndex, thw_actualMenu.menu.tab[i].name);
				actionIndex++;
			}else{
				THW_printf("\r\n");
			}
		}
	}
	THW_printf("\r\n");
	THW_printf("%2d - Back\r\n", thw_cmdBack);
	THW_printf("\r\n");
	THW_printf("Choice :  ");
}

//=============================================================================
//						Externals Functions (API)
//=============================================================================
/******************************************************************************
 ** Function name:		THW_init
 ** Descriptions:		THW component Init
 ******************************************************************************/
thw_status_t THW_init(thw_io_if_t* _io, void (*_entryTestFn)(void*))
{
	if(_io == NULL || _io->init == NULL || _io->readLine == NULL || _io->write == NULL || _io->deinit == NULL)
	    return THW_STATUS_ERROR;

	if(!_io->init())
	    return THW_STATUS_ERROR;

	thw_ctx.io = _io;

	// Check parameter
	if (_entryTestFn == NULL)
		return THW_STATUS_ERROR;

    // Save entry function
    thw_entryTestFn = _entryTestFn;

    	// Creating Task
	thw_tsk_id = osThreadNew(thw_tsk_fn, NULL, &thw_tsk_attr);
	if(thw_tsk_id == NULL){
		return THW_STATUS_ERROR;
	}

	return THW_STATUS_OK;

}

static char console_fmtBuf[256];

//-----------------------------------------------------------------------------
/// \fn 		THW_printf
/// \param[in] fmt : format string (printf style)
/// \return HAL status
/// \brief THW_printf: printf implementation for THW, using transport send function
//-----------------------------------------------------------------------------
thw_status_t THW_printf(const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);

	// build string
	if(vsnprintf(console_fmtBuf, sizeof(console_fmtBuf), fmt, argp) >= 0){
		thw_ctx.io->write(console_fmtBuf);
	}
	va_end(argp);
	return THW_STATUS_OK;
}


#endif //MODE_THW
