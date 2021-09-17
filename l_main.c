/* Emacs style mode select   -*- C++ -*- 
*-----------------------------------------------------------------------------
*
* $Id: l_main.c,v 1.14 2000/03/16 13:27:29 cph Exp $
*
*  Hybrid of the Boom i_main.c and original linuxdoom i_main.c
*
*  LxDoom, a Doom port for Linux/Unix
*  based on BOOM, a modified and improved DOOM engine
*  Copyright (C) 1999 by
*  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
*   and Colin Phipps
*  
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
*  02111-1307, USA.
*
* DESCRIPTION:
*      Startup and quit functions. Handles signals, inits the 
*      memory management, then calls D_DoomMain. Also contains 
*      I_Init which does other system-related startup stuff.
*
*-----------------------------------------------------------------------------
*/

static const char
rcsid[] = "$Id: l_main.c,v 1.14 2000/03/16 13:27:29 cph Exp $";

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "lprintf.h"
#include "m_random.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_misc.h"
#include "i_sound.h"
#include "i_main.h"
#include "l_sdl.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int broken_pipe;

// ptek
extern int forceDisplayMode = -1;
int getDisplayModeFromELFName(char **argv);

/*

// ptek
#include "libpad.h"

int padUtils_Init(int port, int slot);
int padUtils_CheckConnected(int port, int slot);
int padUtils_ReadButton(int port, int slot, u32 old_pad, u32 new_pad);

int waitPadReady(int port, int slot);
int initializePad(int port, int slot);

static char padBuf[256] __attribute__((aligned(64)));
static char actAlign[6];
static int actuators;

*/

/* Most of the following has been rewritten by Lee Killough
*
* I_GetTime
* killough 4/13/98: Make clock rate adjustable by scale factor
* cphipps - much made static
*/

int realtic_clock_rate = 100;
static int_64_t I_GetTime_Scale = 1<<24;

static int I_GetTime_Scaled(void)
{
	return (int_64_t) I_GetTime_RealTime() * I_GetTime_Scale >> 24;
}

static int  I_GetTime_FastDemo(void)
{
	static int fasttic;
	return fasttic++;
}

static int I_GetTime_Error(void)
{
	I_Error("Error: GetTime() used before initialization");
	return 0;
}

int (*I_GetTime)(void) = I_GetTime_Error;

void I_Init(void)
{
	/* killough 4/14/98: Adjustable speedup based on realtic_clock_rate */
	if (fastdemo)
		I_GetTime = I_GetTime_FastDemo;
	else
		if (realtic_clock_rate != 100)
		{
			I_GetTime_Scale = ((int_64_t) realtic_clock_rate << 24) / 100;
			I_GetTime = I_GetTime_Scaled;
		}
		else
			I_GetTime = I_GetTime_RealTime;

	/* Jess 3/00: hack to allow lsdldoom to shutdown cleanly */ 
	I_InitSDL();

	{ 
		/* killough 2/21/98: avoid sound initialization if no sound & no music */
		extern boolean nomusicparm, nosfxparm;
		if (!(nomusicparm && nosfxparm))
			I_InitSound();
	}
}

/* cleanup handling -- killough:
*/
//static void I_SignalHandler(int s)
//{
//	char buf[2048];
//
//#ifdef SIGPIPE
//	/* CPhipps - report but don't crash on SIGPIPE */
//	if (s == SIGPIPE) {
//		fprintf(stderr, "Broken pipe\n");
//		broken_pipe = 1;
//		return;
//	}
//#endif
//	//signal(s,SIG_IGN);  /* Ignore future instances of this signal.*/
//
//	strcpy(buf,"Exiting on signal: ");
//	I_SigString(buf+strlen(buf),2000-strlen(buf),s);
//
//	/* If corrupted memory could cause crash, dump memory
//	* allocation history, which points out probable causes
//	*/
//	if (s==SIGSEGV || s==SIGILL || s==SIGFPE)
//		Z_DumpHistory(buf);
//
//	I_Error(buf);
//}

/* killough 2/22/98: Add support for ENDBOOM, which is PC-specific
*
* this converts BIOS color codes to ANSI codes.  
* Its not pretty, but it does the job - rain
* CPhipps - made static
*/

static inline int convert(int color, int *bold)
{
	if (color > 7) {
		color -= 8;
		*bold = 1;
	}
	switch (color) {
  case 0:
	  return 0;
  case 1:
	  return 4;
  case 2:
	  return 2;
  case 3:
	  return 6;
  case 4:
	  return 1;
  case 5:
	  return 5;
  case 6:
	  return 3;
  case 7:
	  return 7;
	}
	return 0;
}

/* CPhipps - flags controlling ENDOOM behaviour */
enum {
	endoom_colours = 1, 
	endoom_nonasciichars = 2, 
	endoom_droplastline = 4
};

unsigned int endoom_mode;

static void PrintVer(void)
{
	printf("PS2Doom v%s\n",VERSION);
}

/* I_EndDoom
* Prints out ENDOOM or ENDBOOM, using some common sense to decide which.
* cphipps - moved to l_main.c, made static
*/
static void I_EndDoom(void)
{
	int lump_eb, lump_ed, lump = -1;

	/* CPhipps - ENDOOM/ENDBOOM selection */
	lump_eb = W_CheckNumForName("ENDBOOM");/* jff 4/1/98 sign our work    */
	lump_ed = W_CheckNumForName("ENDOOM"); /* CPhipps - also maybe ENDOOM */

	if (lump_eb == -1) 
		lump = lump_ed;
	else if (lump_ed == -1) 
		lump = lump_eb;
	else { /* Both ENDOOM and ENDBOOM are present */
#define LUMP_IS_NEW(num) (!((lumpinfo[num].source == source_iwad) || (lumpinfo[num].source == source_auto_load)))
		switch ((LUMP_IS_NEW(lump_ed) ? 1 : 0 ) | 
			(LUMP_IS_NEW(lump_eb) ? 2 : 0)) {
	case 1:
		lump = lump_ed;
		break;
	case 2:
		lump = lump_eb;
		break;
	default:
		/* Both lumps have equal priority, both present */
		lump = (P_Random(pr_misc) & 1) ? lump_ed : lump_eb;
		break;
		}
	}

	if (lump != -1)
	{
		const char (*endoom)[2] = (void*)W_CacheLumpNum(lump);
		int i, l = W_LumpLength(lump) / 2;

		/* cph - colour ENDOOM by rain */
		int oldbg = 0, oldcolor = 7, bold = 0, oldbold = 0, color = 0;
		if (endoom_mode & endoom_nonasciichars)
			/* switch to secondary charset, and set to cp437 (IBM charset) */
			printf("\e)K\016");

		/* cph - optionally drop the last line, so everything fits on one screen */
		if (endoom_mode & endoom_droplastline)
			l -= 80;
		putchar('\n');
		for (i=0; i<l; i++)
		{
#ifdef DJGPP
			textattr(endoom[i][1]);
#else
			if (endoom_mode & endoom_colours) {
				if (!(i % 80)) {
					/* reset everything when we start a new line */
					oldbg = 0;
					oldcolor = 7;
					printf("\e[0m\n");
				}
				/* foreground color */
				bold = 0;
				color = endoom[i][1] % 16;
				if (color != oldcolor) {
					oldcolor = color;
					color = convert(color, &bold);
					if (oldbold != bold) {
						oldbold = bold;
						oldbg = 0;
					}
					/* we buffer everything or output is horrendously slow */
					printf("\e[%d;%dm", bold, color + 30);
					bold = 0;
				}
				/* background color */
				color = endoom[i][1] / 16; 
				if (color != oldbg) {
					oldbg = color;
					color = convert(color, &bold);
					printf("\e[%dm", color + 40);
				}
			}
			/* cph - portable ascii printout if requested */
			//if (isascii(endoom[i][0]) || (endoom_mode & endoom_nonasciichars))
			//  putchar(endoom[i][0]);
			//else /* Probably a box character, so do #'s */
			putchar('#');
#endif
		}
		putchar('\b');   /* hack workaround for extra newline at bottom of screen */
		putchar('\r');
		if (endoom_mode & endoom_nonasciichars)
			putchar('\017'); /* restore primary charset */
		W_UnlockLumpNum(lump);
	}
	if (endoom_mode & endoom_colours)
		puts("\e[0m"); /* cph - reset colours */
	PrintVer();
}

static int has_exited;

/* I_SafeExit
* This function is called instead of exit() by functions that might be called 
* during the exit process (i.e. after exit() has already been called)
* Prevent infinitely recursive exits -- killough
*/

void I_SafeExit(int rc)
{
	if (!has_exited)    /* If it hasn't exited yet, exit now -- killough */
	{
		has_exited=rc ? 2 : 1;   
		exit(rc);
	}
}

void I_Quit (void)
{
    int i;

	if (!has_exited)
		has_exited=1;   /* Prevent infinitely recursive exits -- killough */

	if (has_exited == 1) {
		I_EndDoom();
		if (demorecording)
			G_CheckDemoStatus();
        
        // ptek : Display warning

        init_scr();
        scr_setXY(26,12);       // y coordinate for non-interlaced PAL screen
        scr_printf("Saving configuration to memory card 1\n\n\n                       PLEASE WAIT! DO NOT POWER OFF THE CONSOLE!");

        //

        M_SaveDefaults ();

        // ptek : Addtional save wait
        for (i=0; i<50*10; i++)
            gsKit_vsync();        

        init_scr();
        scr_setXY(21,15);
        scr_printf("Configuration saved. You may now turn off your PS2");        
	}
}


/*Declare usbd module */
extern unsigned char usbd[];
extern unsigned int size_usbd;
/*Declare usbhdfsd module */
extern unsigned char usbhdfsd[];
extern unsigned int size_usbhdfsd;
#include <sifrpc.h>

void LoadModule(char *path, int argc, char *argv) {
	int ret;

	ret = SifLoadModule(path, argc, argv);
	if (ret < 0) {
		printf("Could not load module %s: %d\n", path, ret);
		SleepThread();
	}
}

//#include <romfs_io.h>       // ptek : uncommented for romfs support

int main(int argc, char **argv)
{
	int ret;

    //int butres = -1, id = -1;        // ptek
    forceDisplayMode = getDisplayModeFromELFName(argv);

	//rioInit();      // ptek : uncommented for romfs support
	/* Version info */
	putchar('\n');
	PrintVer();

	myargc = argc;
	myargv = (const char* const *)argv;

	SifInitRpc(0);

	LoadModule("rom0:XSIO2MAN", 0, NULL);
	LoadModule("rom0:XMCMAN", 0, NULL);
	LoadModule("rom0:XMCSERV", 0, NULL);

    //// ptek : to read the joypad for optional forcing display mode
    //LoadModule("rom0:XPADMAN", 0, NULL);

	SifExecModuleBuffer(usbd, size_usbd, 0, NULL, &ret);
	SifExecModuleBuffer(usbhdfsd, size_usbhdfsd, 0, NULL, &ret);

	/*
	killough 1/98:

	This fixes some problems with exit handling
	during abnormal situations.

	The old code called I_Quit() to end program,
	while now I_Quit() is installed as an exit
	handler and exit() is called to exit, either
	normally or abnormally. Seg faults are caught
	and the error handler is used, to prevent
	being left in graphics mode or having very
	loud SFX noise because the sound card is
	left in an unstable state.
	*/

	Z_Init();                  /* 1/18/98 killough: start up memory stuff first */

	atexit(I_Quit);

	/* cphipps - call to video specific startup code */
	I_PreInitGraphics();

	/* 2/2/98 Stan
	* Must call this here.  It's required by both netgames and i_video.c.
	*/

    // ptek
    if (forceDisplayMode != -1)
        PS2SDL_ForceSignal(forceDisplayMode);

    /*
    // ptek
    init_scr();
    scr_setXY(13,15);
    scr_printf("Hold L1 to force PAL display, R1 for NTSC. Ignore this for AUTO\n\n");
    
    // ptek : check for display forcing options
    butres = ReadPadButton();
    init_scr();
    if (butres != -1);
    {
        
        scr_setXY(13,15);
        
        if (butres == PAD_L1)
        {
            scr_printf("Forcing PAL display mode");
            PS2SDL_ForceSignal(0);
        }
        else
        {    if (butres == PAD_R1)
        {
            scr_printf("Forcing NTSC display mode");
            PS2SDL_ForceSignal(1);
        }
        }
        scr_printf("\n\n");
    }
    
    id = SifSearchModuleByName("XPADMAN");
    printf("id : %d\n", id);
    id = SifUnloadModule(id);
    printf("SifUnloadModule : %d\n", id);
*/	

    D_DoomMain();
	return 0;
}


// ptek
//
// Check if ELF name ends with PAL, pal, NTSC, ntsc.
int getDisplayModeFromELFName(char **argv)
{
    char tmp[10];
    int ln = 0,i,j;

    ln = strlen(argv[0]);
    if (ln>=8)
    {
        // check for PAL
        j=0;
        for(i=ln-7; i<=ln-5; i++)
            tmp[j++] = argv[0][i];
        tmp[j] = 0;

        if (strcmp(tmp, "PAL") == 0 || strcmp(tmp, "pal") == 0)
            return 0;
        
        // check for NTSC
        j=0;
        for(i=ln-8; i<=ln-5; i++)
            tmp[j++] = argv[0][i];
        tmp[j] = 0;

        if (strcmp(tmp, "NTSC") == 0 || strcmp(tmp, "ntsc") == 0)
            return 1;
    }

    return -1;
}

/*

// ptek
int ReadPadButton()
{
    int port, slot;
    int butres = -1;

    u32 old_pad = 0;
    u32 new_pad = 0;
    padInit(0);

    port = 0; // 0 -> Connector 1, 1 -> Connector 2
    slot = 0; // Always zero if not using multitap

    padUtils_Init(port, slot);
    
    butres = padUtils_ReadButton(port, slot, old_pad, new_pad);

    return butres;
}


////////
// Reads a pad button without blocking and returns its id
//
int padUtils_ReadButton(int port, int slot, u32 old_pad, u32 new_pad)
{
    struct padButtonStatus buttons;
    int ret;
    u32 paddata;

    ret = padRead(port, slot, &buttons);
    if (ret != 0)
    {
        paddata = 0xffff ^ buttons.btns;
        
        new_pad = paddata & ~old_pad;
        old_pad = paddata;
        

        if (new_pad & PAD_LEFT)
        {
            //scr_printf("LEFT\n");
            return(PAD_LEFT);
        }
        if (new_pad & PAD_DOWN)
        {
            //scr_printf("DOWN\n");
            return(PAD_DOWN);
        }
        if (new_pad & PAD_RIGHT)
        {
            //scr_printf("RIGHT\n");
            return(PAD_RIGHT);
        }
        if (new_pad & PAD_UP)
        {
            //scr_printf("UP\n");
            return(PAD_UP);
        }
        if (new_pad & PAD_START)
        {
            //scr_printf("START\n");
            return(PAD_START);
        }
        if (new_pad & PAD_R3)
        {
            //scr_printf("R3\n");
            return(PAD_R3);
        }
        if (new_pad & PAD_L3)
        {
            //scr_printf("L3\n");
            return(PAD_L3);
        }
        if (new_pad & PAD_SELECT)
        {
            //scr_printf("SELECT\n");
            return(PAD_SELECT);
        }
        if (new_pad & PAD_SQUARE)
        {
            //scr_printf("SQUARE\n");
            return(PAD_SQUARE);
        }
        if (new_pad & PAD_CROSS)
        {
            //scr_printf("CROSS\n");
            return(PAD_CROSS);
        }
        if (new_pad & PAD_CIRCLE)
        {
            //scr_printf("CIRCLE\n");
            return(PAD_CIRCLE);
        }
        if (new_pad & PAD_TRIANGLE)
        {
            //scr_printf("TRIANGLE\n");
            return(PAD_TRIANGLE);
        }
        if (new_pad & PAD_R1)
        {
            //scr_printf("R1\n");
            return(PAD_R1);
        }
        if (new_pad & PAD_L1)
        {
            //scr_printf("L1\n");
            return(PAD_L1);
        }
        if (new_pad & PAD_R2)
        {
            //scr_printf("R2\n");
            return(PAD_R2);
        }
        if (new_pad & PAD_L2)
        {
            //scr_printf("L2\n");
            return(PAD_L2);
        }
    }
    else
        return -1;
    
    return 0;   // 0 means no button was pressed
}

int waitPadReady(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
            printf("Please wait, pad(%d,%d) is in state %s\n", 
                       port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        printf("Pad OK!\n");
    }
    return 0;
}



/////
int padUtils_Init(int port, int slot)
{
    int i = 0, ret;
    if ((ret = padPortOpen(port, slot, padBuf)) == 0)
    {
        printf("padOpenPort failed: %d\n", ret);
        return -1;
    }

    if (!initializePad(port, slot))
    {
        printf("pad initalization failed!\n");
        return -2;
    }

    i = padUtils_CheckConnected(port, slot);
    if (i==0)
        return -3;

    return 0;
}


/////////
int initializePad(int port, int slot)
{

    int ret;
    int modes;
    int i;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
    printf("The device has %d modes\n", modes);

    if (modes > 0) {
        printf("( ");
        for (i = 0; i < modes; i++) {
            printf("%d ", padInfoMode(port, slot, PAD_MODETABLE, i));
        }
        printf(")");
    }

    printf("It is currently using mode %d\n", 
               padInfoMode(port, slot, PAD_MODECURID, 0));

    // If modes == 0, this is not a Dual shock controller 
    // (it has no actuator engines)
    if (modes == 0) {
        printf("This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
        printf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
        printf("This is no Dual Shock controller??\n");
        return 1;
    }

    printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
    printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    waitPadReady(port, slot);        
    printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
    printf("# of actuators: %d\n",actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
        printf("padSetActAlign: %d\n", 
                   padSetActAlign(port, slot, actAlign));
    }
    else {
        printf("Did not find any actuators.\n");
    }

    waitPadReady(port, slot);

    return 1;
}


/////////////
int padUtils_CheckConnected(int port, int slot)
{
    int i=0, ret;
    ret=padGetState(port, slot);
    while ((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1))
    {
        if (ret==PAD_STATE_DISCONN)
        {
            printf("Pad(%d, %d) is disconnected\n", port, slot);
            return 0;
        }
        ret=padGetState(port, slot);
    }
    if (i==1)
    {
        printf("Pad: OK!\n");
    }
    return 1;
}


*/

/*----------------------------------------------------------------------------
*
* $Log: l_main.c,v $
* Revision 1.14  2000/03/16 13:27:29  cph
* Clean up uid stuff
*
* Revision 1.13  2000/01/25 21:33:22  cphipps
* Fix security in case of being setuid
*
* Revision 1.12  1999/11/01 00:20:11  cphipps
* Change int64_t's to int_64_t's
* (the latter being LxDoom's wrapper for 64 bit int type)
*
* Revision 1.11  1999/10/31 16:07:38  cphipps
* Moved most system functions that are specifically for the doom game from
* l_system.c to here.
* Cahnged all C++ comments to C.
* Use new functions in l_system.c to get the LxDoom ver and signal names.
* New function I_SafeExit is a wrapper for exit() that checks has_exited.
*
* Revision 1.10  1999/10/12 13:01:11  cphipps
* Changed header to GPL
*
* Revision 1.9  1999/07/03 13:15:07  cphipps
* Add broken_pipe variable to allow for broken pipe checking
*
* Revision 1.8  1999/06/20 14:04:13  cphipps
* Code cleaning
*
* Revision 1.7  1999/01/11 16:03:37  cphipps
* Fix version string printout
*
* Revision 1.6  1998/11/17 16:40:06  cphipps
* Modified to work for DosDoom and LxDoom
*
* Revision 1.5  1998/10/16 22:20:50  cphipps
* Match argv to myargv in type const char* const *
* Disable dodgy BOOMPATH hack to fix D_DoomExeDir remotely, since it writes argv[0]
*
* Revision 1.4  1998/10/15 20:13:02  cphipps
* Made SIGPIPE non-fatal
*
* Revision 1.3  1998/10/13 11:52:29  cphipps
* Added i_video.h and I_PreInitGraphics call
*
* Revision 1.2  1998/09/23 09:34:53  cphipps
* Added identifying string at startup.
* Add code to patch up myargv[0].
* Cleaned up exit handling
* Removed allegro_init call
*
* Revision 1.1  1998/09/13 16:49:50  cphipps
* Initial revision
*
* Revision 1.8  1998/05/15  00:34:03  killough
* Remove unnecessary crash hack
*
* Revision 1.7  1998/05/13  22:58:04  killough
* Restore Doom bug compatibility for demos
*
* Revision 1.6  1998/05/03  22:38:36  killough
* beautification
*
* Revision 1.5  1998/04/27  02:03:11  killough
* Improve signal handling, to use Z_DumpHistory()
*
* Revision 1.4  1998/03/09  07:10:47  killough
* Allow CTRL-BRK during game init
*
* Revision 1.3  1998/02/03  01:32:58  stan
* Moved __djgpp_nearptr_enable() call from I_video.c to i_main.c
*
* Revision 1.2  1998/01/26  19:23:24  phares
* First rev with no ^Ms
*
* Revision 1.1.1.1  1998/01/19  14:02:57  rand
* Lee's Jan 19 sources
*
*----------------------------------------------------------------------------*/
