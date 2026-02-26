
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


static struct
{
	// Internal state
	bool isStarted;

	// Dependency Injection
	thw_io_if_t* 	io;
	thw_time_if_t* 	time;

	// Refresh Task management
	uint32_t lastRefreshTick;

	// Menu management
	st_thw_menu currentMenu;

}thw_ctx;



//------------------------------------------------------------------------------
// MENU
//------------------------------------------------------------------------------

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
		thw_ctx.currentMenu.refreshFn = NULL;

		// Call menu-specific exit handler if any
		if (thw_ctx.currentMenu.onExit){
			// Save onExit pointer
			void (*onExit_save)(void) = thw_ctx.currentMenu.onExit;

			// Clear current menu to avoid re-entrance issues
			memset(&thw_ctx.currentMenu, 0, sizeof(st_thw_menu));

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

	for (uint16_t i = 0; i < thw_ctx.currentMenu.menu.size; i++)
	{
		const st_thw_menuItem* item = &thw_ctx.currentMenu.menu.tab[i];

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
	if (thw_ctx.currentMenu.menu.tab != NULL && thw_ctx.currentMenu.menu.size > 0)
	{
		uint16_t actionIndex = 1;
		for (uint16_t i = 0; i < thw_ctx.currentMenu.menu.size; i++)
		{
			if(thw_ctx.currentMenu.menu.tab[i].pActionFn != NULL){
				THW_printf("%2d - %s\r\n", actionIndex, thw_ctx.currentMenu.menu.tab[i].name);
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
//-----------------------------------------------------------------------------
/// \fn 		THW_init
/// \param[in] _io : pointer to io interface struct (see thw_io_if_t)
/// \param[in] _time : pointer to time interface struct (see thw_time_if_t)
/// \param[in] _entryTestFn : pointer to test entry function, called at the first call of THW_process() to set the first menu
/// \return HAL status
/// \brief THW_init: initializes THW with provided dependencies and entry point
//-----------------------------------------------------------------------------
thw_status_t THW_init(thw_io_if_t* _io, thw_time_if_t* _time, void (*_entryTestFn)(void*))
{
	memset(&thw_ctx, 0, sizeof(thw_ctx));

	thw_ctx.isStarted = false;

	// check io interface
	if(_io == NULL || _io->init == NULL || _io->readLine == NULL || _io->write == NULL || _io->deinit == NULL)
	    return THW_STATUS_ERROR;

	// check time interface
	if(_time == NULL || _time->getTickMs == NULL)
	    return THW_STATUS_ERROR;

	// check entry function
	if(_entryTestFn == NULL)
	    return THW_STATUS_ERROR;

	// init io interface
	if(!_io->init())
	    return THW_STATUS_ERROR;

	// Dependency Injection
	thw_ctx.io = _io;
	thw_ctx.time = _time;

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
    static char rxLine[128];

    if (!thw_ctx.isStarted)
    {
        THW_printf(VT100_NORMAL);

        if (thw_entryTestFn)
            thw_entryTestFn(NULL);

        thw_ctx.isStarted = true;
    }

    // --- Input
    if (thw_ctx.io->readLine(rxLine, sizeof(rxLine)))
    {
        uint32_t userChoice = atoi(rxLine);
        manageChoice(userChoice);
    }

    // --- Refresh
    if (thw_ctx.currentMenu.refreshFn)
    {
        uint32_t now = thw_ctx.time->getTickMs();

        if (now - thw_ctx.lastRefreshTick >= thw_ctx.currentMenu.refreshPeriodInMs)
        {
            thw_ctx.lastRefreshTick = now;
            THW_saveCurPos();
            thw_ctx.currentMenu.refreshFn();
            THW_restoreCurPos();
        }
    }
}

//-----------------------------------------------------------------------------
/// \fn     void THW_setMenu(const st_thw_actualMenu* menu)
/// \brief  THW menu setter, to be called by test code to set the current menu
//-----------------------------------------------------------------------------
void THW_setMenu(const st_thw_menu* menu)
{
    if (menu == NULL || menu->displayMenu == NULL)
        return;

    thw_ctx.currentMenu = *menu;

    if (thw_ctx.currentMenu.onInit)
        thw_ctx.currentMenu.onInit();

    THW_clearScreen();

    if (thw_ctx.currentMenu.displayMenu)
        thw_ctx.currentMenu.displayMenu();

    displayMenuAction();

    thw_ctx.lastRefreshTick = thw_ctx.time->getTickMs();
}


#endif //MODE_THW
