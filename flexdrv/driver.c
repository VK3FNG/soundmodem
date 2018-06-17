#define  STRICT
#include <windows.h>
#pragma hdrstop

#include <process.h>
#include "flexdrv.h" // der offizielle Header
#include "drv32.h"

HANDLE hInst;
//--------------------------------------------------------------------
// Einziger Treiberaufruf
u32 flexnet_driver(int func, int kanal, int a, int b)
	{	
	static byte base_kanal;
	static byte tx_kanal;
	static L1FRAME *l1f;
	L1FRAME *rxf;
	switch (func)
		{
		case drv_interfaceversion:
							return DRIVER_INTERFACE_VERSION;
		case drv_ident:		return (u32)l1_ident((byte)(kanal-base_kanal));
		case drv_version:	return (u32)l1_version((byte)(kanal - base_kanal));
		case drv_config:	return config_device((byte)(kanal), (HWND)a, (byte)(b - base_kanal));
		case drv_confinfo:	return (u32)config_info((byte)(kanal - base_kanal));
		case drv_init_device:
			base_kanal = kanal;
			return init_device((HKEY)b);
		case drv_get_ch_cnt:		return l1_get_ch_cnt();
		case drv_exit:				l1_exit((HKEY)a); break;
		case drv_ch_active:			return l1_ch_active((byte)(kanal - base_kanal));
		case drv_init_kanal:		return l1_init_kanal((byte)(kanal - base_kanal), (u16)a, (u16)b);
		case drv_stat:				return (u32)l1_stat((byte)(kanal - base_kanal), (byte)a);
		case drv_ch_state:			return l1_ch_state((byte)(kanal - base_kanal));
		case drv_scale:				return l1_scale((byte)(kanal - base_kanal));
		case drv_tx_calib:			l1_tx_calib((byte)(kanal - base_kanal), (byte)a); break;
		case drv_set_led:			set_led((byte)(kanal - base_kanal), (byte)a); break;
		case drv_rx_frame:
			if (rxf = l1_rx_frame())
				rxf->kanal += base_kanal;
			return (u32)rxf;
		case drv_get_framebuf:		return (u32)(l1f = l1_get_framebuf(tx_kanal = (byte)(kanal - base_kanal)));
		case drv_tx_frame:
			if (l1f) l1f->kanal = tx_kanal;
			return l1_tx_frame();
		case drv_get_txdelay:	return get_txdelay((byte)(kanal - base_kanal));
		case drv_get_mode:		return get_mode((byte)(kanal - base_kanal));
		case  drv_get_baud:		return get_baud((byte)(kanal - base_kanal));
		case drv_set_txdelay:	set_txdelay((byte)(kanal - base_kanal), (byte)a); break;
		default:
			return 0;
		}
	return 0;
	}
//--------------------------------------------------------------------
BOOL WINAPI DllMain (HANDLE hModule, DWORD dwFunction, LPVOID lpNot)
	{
	switch(dwFunction)
		{
		case DLL_PROCESS_ATTACH:
			// hier evtl. noch Test ob mehrfach geladen, dazu brauchts aber Handle (mutex oder sowas)
			hInst = hModule;
			// kein break, auch neuer Prozess istn Thread
		case DLL_THREAD_ATTACH:
			break;
		case DLL_PROCESS_DETACH:
			// vor allem wenn harter Abbruch: Alles deinitialisieren!
			break; // kein Durchfall, Prozessende gibt eh alles frei und schliesst pipes
		case DLL_THREAD_DETACH:
			break;
		}
	return 1;   // Indicate that the DLL was initialized successfully.
	}
//--------------------------------------------------------------------
