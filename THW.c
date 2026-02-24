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

#include <TestHardwareCore/include/thw_transport_if.h>

#include "THW_api.h"
#include <Config/task_config.h>


static struct{
	// Dependency Injection
	thw_transport_if_t *transport;

	// Auto Refresh management
	bool skipAutoRefresh;


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

//-----------------------------------------------------------------------------
// THREAD Refresh
//-----------------------------------------------------------------------------
osThreadId_t thw_rfs_tsk_id;
const osThreadAttr_t thw_rfs_tsk_attr = {
		.name = "thw_rfs_tsk",
		.stack_size = TSK_CFG__STACK__TSK_THW_RFS,
		.priority = TSK_CFG__PRIO__TSK_THW_RFS,
};
static void thw_rfs_tsk_fn(void *arg);
bool 		thw_refreshIsRunning = false;
static HAL_StatusTypeDef thw_startRefreshTask(void);
static HAL_StatusTypeDef thw_stopRefreshTask(void);

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

		// Raz Auto Refresh flag
		thw_ctx.skipAutoRefresh = false;

		// Get User Choice
		//----------------------
		if(console_manageRx() == true){

			// 	Manage user Choice
			//----------------------
			userChoice = atoi((char*)console_rxBuffer);

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
			if (thw_ctx.skipAutoRefresh == false){
				THW_clearScreen();
				if(thw_actualMenu.displayMenu != NULL)
					thw_actualMenu.displayMenu();
				displayMenuAction();
			}else{
				THW_restoreCurPos();			// Restoration position curseur
				THW_printf(VT100_CLEAREOL);		// Clear to end of line
				THW_restoreCurPos();			// Restoration position curseur
			}

			// 		Refresh
			//----------------------
			if(thw_actualMenu.refreshFn != NULL)
				thw_startRefreshTask();
			else
				thw_stopRefreshTask();
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
			thw_ctx.skipAutoRefresh = item->skipAutoRefresh;
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
	THW_saveCurPos();
}

//------------------------------------------------------------------------------
/// 								Refresh Thread
//------------------------------------------------------------------------------
static void thw_rfs_tsk_fn(void *arg)
{
	thw_refreshIsRunning = true;
	while(thw_refreshIsRunning == true)
	{
		// Si mode refresh actif
		if(thw_actualMenu.refreshFn != NULL){
			THW_saveCurPos();				// Sauvegarde position curseur
			thw_actualMenu.refreshFn();		// Execution fonction Refresh
			THW_restoreCurPos();			// Restoration position curseur
		}

		// Période
		if(thw_actualMenu.refreshPeriodInMs < 50)
			osDelay(50);
		else if (thw_actualMenu.refreshPeriodInMs > 2000)
			osDelay(2000);
		else
			osDelay(thw_actualMenu.refreshPeriodInMs);
	}
	osThreadTerminate(thw_rfs_tsk_id);
}


//=============================================================================
//						Externals Functions (API)
//=============================================================================
/******************************************************************************
 ** Function name:		THW_init
 ** Descriptions:		THW component Init
 ******************************************************************************/
HAL_StatusTypeDef THW_init(thw_transport_if_t *transport, void (*_entryTestFn)(void*))
{
	if(transport == NULL)
		return HAL_ERROR;

	// Save transport interface
	thw_ctx.transport = transport;

	// Check parameter
	if (_entryTestFn == NULL)
		return HAL_ERROR;

	// Low Level Init
	if(thw_ctx.transport->init == NULL || !thw_ctx.transport->init()){
		return HAL_ERROR;
	}

    // Save entry function
    thw_entryTestFn = _entryTestFn;

    	// Creating Task
	thw_tsk_id = osThreadNew(thw_tsk_fn, NULL, &thw_tsk_attr);
	if(thw_tsk_id == NULL){
		return HAL_ERROR;
	}

	return HAL_OK;

}

static char console_fmtBuf[256];

//-----------------------------------------------------------------------------
/// \fn 		THW_printf
/// \param[in] fmt : format string (printf style)
/// \return HAL status
/// \brief THW_printf: printf implementation for THW, using transport send function
//-----------------------------------------------------------------------------
HAL_StatusTypeDef THW_printf(const char *fmt, ...)
{
	va_list argp;
	int len;

	va_start(argp, fmt);

	// build string
	if(vsnprintf(console_fmtBuf, sizeof(console_fmtBuf), fmt, argp) >= 0){
		len = strlen((char*)console_fmtBuf);
		thw_ctx.transport->write((uint8_t*)console_fmtBuf, len);
	}
	va_end(argp);
	return HAL_OK;
}


/******************************************************************************
 ** Function name:		thw_startRefreshTask
 ** Descriptions:		Démarre la tâche Refresh
 ** parameters:			NA
 ** Returned value:		Status
 ******************************************************************************/
static HAL_StatusTypeDef thw_startRefreshTask(void)
{
	if(thw_refreshIsRunning != true){
		// Création de la tâche Refresh
		//----------------------------------
		thw_rfs_tsk_id = osThreadNew(thw_rfs_tsk_fn, NULL, &thw_rfs_tsk_attr);
		if(thw_rfs_tsk_id == NULL){
			return(HAL_ERROR);
		}
	}

	return(HAL_OK);
}

/******************************************************************************
 ** Function name:		thw_stopRefreshTask
 ** Descriptions:		Arrêt de la tâche Refresh
 ** parameters:			NA
 ** Returned value:		Status
 ******************************************************************************/
static HAL_StatusTypeDef thw_stopRefreshTask(void)
{
	thw_refreshIsRunning = false;
	return HAL_OK;
}


#endif //MODE_THW
