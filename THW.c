
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

#include "THW_api.h"
#include <Config/task_config.h>
#include <TestHardwareCore/interfaces/thw_io_if.h>
#include <TestHardwareCore/interfaces/thw_time_if.h>


static struct{
	// Dependency Injection
	thw_io_if_t* 	io;
	thw_time_if_t* 	time;

	// Refresh Task management
	uint32_t lastRefreshTick;
}thw_ctx;



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
thw_status_t THW_init(thw_io_if_t* _io, thw_time_if_t* _time, void (*_entryTestFn)(void*))
{
	// check io interface
	if(_io == NULL || _io->init == NULL || _io->readLine == NULL || _io->write == NULL || _io->deinit == NULL)
	    return THW_STATUS_ERROR;

	// check time interface
	if(_time == NULL || _time->getTickMs == NULL)
	    return THW_STATUS_ERROR;

	// init io interface
	if(!_io->init())
	    return THW_STATUS_ERROR;

	// Dependency Injection
	thw_ctx.io = _io;
	thw_ctx.time = _time;

	// Check parameter
	if (_entryTestFn == NULL)
		return THW_STATUS_ERROR;

    // Save entry function
    thw_entryTestFn = _entryTestFn;

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

//-----------------------------------------------------------------------------
/// \fn     void THW_process(void)
/// \brief  THW main process, to be called cyclically by the platform runner
//-----------------------------------------------------------------------------
void THW_process(void)
{
    static bool initialized = false;
    static char rxLine[128];

    if (!initialized)
    {
        THW_printf(VT100_NORMAL);

        if (thw_entryTestFn)
            thw_entryTestFn(NULL);

        if (thw_actualMenu.onInit)
            thw_actualMenu.onInit();

        THW_clearScreen();

        if (thw_actualMenu.displayMenu)
            thw_actualMenu.displayMenu();

        displayMenuAction();

        thw_ctx.lastRefreshTick = thw_ctx.time->getTickMs();

        initialized = true;
    }

    // --- Input
    if (thw_ctx.io->readLine(rxLine, sizeof(rxLine)))
    {
        uint32_t userChoice = atoi(rxLine);

        const void* prevMenuTab  = thw_actualMenu.menu.tab;
        uint16_t    prevMenuSize = thw_actualMenu.menu.size;

        manageChoice(userChoice);

        bool menuChanged =
            (prevMenuTab  != thw_actualMenu.menu.tab) ||
            (prevMenuSize != thw_actualMenu.menu.size) ||
            (userChoice == 0);

        if (menuChanged && thw_actualMenu.onInit)
            thw_actualMenu.onInit();

        THW_clearScreen();

        if (thw_actualMenu.displayMenu)
            thw_actualMenu.displayMenu();

        displayMenuAction();

        thw_ctx.lastRefreshTick = thw_ctx.time->getTickMs();
    }

    // --- Refresh
    if (thw_actualMenu.refreshFn)
    {
        uint32_t now = thw_ctx.time->getTickMs();

        if (now - thw_ctx.lastRefreshTick >= thw_actualMenu.refreshPeriodInMs)
        {
            thw_ctx.lastRefreshTick = now;
            THW_saveCurPos();
            thw_actualMenu.refreshFn();
            THW_restoreCurPos();
        }
    }
}

#endif //MODE_THW
