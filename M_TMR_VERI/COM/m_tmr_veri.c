/****************************************************************************
 ************                                                    ************
 ************                    M_TMR_VERI                      ************
 ************                                                    ************
 ****************************************************************************
 *
 *       Author: kp
 *
 *  Description: Verification tool for MDIS drivers implementing TMR profile
 *
 *  Assumes that one round trip of timer is at least 1second
 *
 *     Required: libraries: mdis_api, usr_oss, usr_utl
 *     Switches: -
 *
 *---------------------------------------------------------------------------
 * Copyright 1999-2019, MEN Mikro Elektronik GmbH
 ******************************************************************************/
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <MEN/men_typs.h>
#include <MEN/usr_oss.h>
#include <MEN/usr_utl.h>
#include <MEN/mdis_api.h>
#include <MEN/m_tmr_drv.h>

static const char IdentString[]=MENT_XSTR(MAK_REVISION);

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   TYPDEFS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
static volatile int32 G_sigCnt;

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
static void usage(void);
static void PrintMdisError(char *info);
static void PrintUosError(char *info);


/********************************* usage ************************************
 *
 *  Description: Print program usage
 *
 *---------------------------------------------------------------------------
 *  Input......: -
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void usage(void)
{
	printf("Usage: m_tmr_veri [<opts>] <device> [<opts>]\n");
	printf("Function: Verification tool for MDIS drivers implementing TMR profile\n");
	printf("  device       device name..................... [none]    \n");
	printf("Options:\n");
	printf("  -c=<dec>     channel number...................[1]       \n");
	printf("\n");
	printf("Copyright 1999-2019, MEN Mikro Elektronik GmbH\n%s\n", IdentString);
}

/********************************* SigHandler ********************************
 *
 *  Description: Handle Signals
 *
 *
 *---------------------------------------------------------------------------
 *  Input......: sigCode		signal code
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void __MAPILIB SigHandler( u_int32 sigCode )
{
	G_sigCnt++;
}

/********************************* main *************************************
 *
 *  Description: Program main function
 *
 *---------------------------------------------------------------------------
 *  Input......: argc,argv	argument counter, data ..
 *  Output.....: return	    success (0) or error (1)
 *  Globals....: -
 ****************************************************************************/
int main(int argc, char *argv[])
{
	MDIS_PATH	path=0;
	int32		chan,value,n,timerBits, timerResolution, tmrTicks;
	char		*device,*errstr,buf[40],*str;
	u_int32		startTime, endTime, maxTimerVal;

	/*--------------------+
    |  check arguments    |
    +--------------------*/
	if ((errstr = UTL_ILLIOPT("c=?", buf))) {	/* check args */
		printf("*** %s\n", errstr);
		return(1);
	}

	if (UTL_TSTOPT("?")) {						/* help requested ? */
		usage();
		return(1);
	}

	/*--------------------+
    |  get arguments      |
    +--------------------*/
	for (device=NULL, n=1; n<argc; n++)
		if (*argv[n] != '-') {
			device = argv[n];
			break;
		}

	if (!device) {
		usage();
		return(1);
	}

	G_sigCnt = 0;

	chan 	  = ((str = (UTL_TSTOPT("c="))) ? atoi(str) : 1 );

	/*--------------------+
    |  open path          |
    +--------------------*/
	if ((path = M_open(device)) < 0) {
		PrintMdisError("open");
		return(1);
	}

	if( UOS_SigInit( SigHandler ) < 0 ){
		PrintUosError("UOS_SigInit");
		goto abort;
	}

	if( UOS_SigInstall( UOS_SIG_USR1 ) < 0 ){
		PrintUosError("UOS_SigInstall");
		goto abort;
	}

	/*--- enable global irqs ---*/
	M_setstat( path, M_MK_IRQ_ENABLE, TRUE );

	/*--- setup current channel ---*/
	if( M_setstat( path, M_MK_CH_CURRENT, chan )){
		PrintMdisError("set current channel");
		goto abort;
	}

	/*--- query profile ---*/
	if( M_getstat( path, M_LL_CH_TYP, &value )){
		PrintMdisError("get channel type");
		goto abort;
	}

	if( value != M_CH_PROFILE_TMR ){
		fprintf(stderr,"Sorry. Channel %d does not implement timer profile\n",
				chan );
		goto abort;
	}


	/*-- query info ---*/
	if( M_getstat( path, M_LL_CH_LEN, &timerBits )){
		PrintMdisError("get channel len");
		goto abort;
	}
	printf("%d bit timer, ", timerBits );

	if( M_getstat( path, M_TMR_RESOLUTION, &timerResolution )){
		PrintMdisError("get timer resolution");
		goto abort;
	}
	printf("%d decrements per second.\n", timerResolution );


	if( M_setstat( path, M_TMR_SIGSET_ZERO, UOS_SIG_USR1 ) < 0 ){
		PrintMdisError("install signal");
		goto abort;
	}

	maxTimerVal = 1L<<timerBits;
	maxTimerVal--;
	/*----------------------------------+
	|  Test duration of one-shot timer  |
	+----------------------------------*/

	tmrTicks = timerResolution;

	printf("Testing timer duration...\n");


	if( M_write( path, tmrTicks ) < 0 ){
		PrintMdisError("write preload");
		goto abort;
	}

	G_sigCnt = 0;

	startTime = UOS_MsecTimerGet();

	if( M_setstat( path, M_TMR_RUN, M_TMR_START_ONE_SHOT ) < 0 )
		PrintMdisError("start timer");

	while( G_sigCnt == 0 );

	endTime = UOS_MsecTimerGet();

	printf("  ms elapsed: %d, Should be: %d\n", endTime-startTime,
		   tmrTicks * 1000 / timerResolution );


	/*---------------------+
	|	Start/Stop Timer   |
	+---------------------*/
	printf("Testing start/stop timer...\n");

	if( M_write( path, maxTimerVal ) < 0 ){
		PrintMdisError("write preload");
		goto abort;
	}

	startTime = UOS_MsecTimerGet();

	if( M_setstat( path, M_TMR_RUN, M_TMR_START_ONE_SHOT ) < 0 )
		PrintMdisError("start timer");

	do {
		if( M_read( path, &tmrTicks ) < 0 )
			PrintMdisError("read timer");

	} while( (u_int32)(tmrTicks) > (maxTimerVal - timerResolution) );

	endTime = UOS_MsecTimerGet();

	if( M_setstat( path, M_TMR_RUN, M_TMR_STOP ) < 0 )
		PrintMdisError("start timer");

	printf("  ms elapsed: %d, Should be: %d\n", endTime-startTime,
		   1000 );

	/* read again to check if counter really stopped */
	{
		u_int32 tmrTicks2;

		if( M_read( path, &tmrTicks ) < 0 )
			PrintMdisError("read timer");
		UOS_Delay(100);

		if( M_getstat( path, M_TMR_RUN, &value ) < 0 )
			PrintMdisError("get timer state");

		if( M_read( path, (int32 *)&tmrTicks2 ) < 0 )
			PrintMdisError("read timer");

		if( tmrTicks2 != tmrTicks ){
			printf("*** timer ticks did not stop %d <> %d\n",
				   tmrTicks2, tmrTicks );
		}
		if( value != 0 ){
			printf("*** timer did not stop\n");
		}
	}


	/*--------------------+
	|  Periodic timer     |
	+--------------------*/
	G_sigCnt = 0;
	tmrTicks = timerResolution / 100;

	printf("Generating periodic signals (100 per second)\n");

	if( M_write( path, tmrTicks ) < 0 ){
		PrintMdisError("write preload");
		goto abort;
	}


	if( M_setstat( path, M_TMR_RUN, M_TMR_START_FREE_RUNNING ) < 0 )
		PrintMdisError("start timer");

	for( n=0; n<10; n++ ){

		startTime = UOS_MsecTimerGet();
		UOS_Delay( 1000 );		/* wait 1s */

		/*--- check how many signals arrived ---*/
		UOS_SigMask();
		value = G_sigCnt;
		G_sigCnt = 0;
		UOS_SigUnMask();
		endTime = UOS_MsecTimerGet();

		printf("  %d signals in %d ms\n", value, endTime-startTime );

	}

	/*--------------------+
    |  cleanup            |
    +--------------------*/
abort:
	/*--- disable global irqs ---*/
	M_setstat( path, M_MK_IRQ_ENABLE, FALSE );

	if( M_setstat( path, M_TMR_SIGCLR_ZERO, 0 ) < 0 )
		PrintMdisError("remove signal");

	if( UOS_SigRemove( UOS_SIG_USR1 ) < 0 ){
		PrintUosError("UOS_SigInstall");
	}

	if( UOS_SigExit() < 0 ){
		PrintUosError("UOS_SigExit");
	}


	if (M_close(path) < 0)
		PrintMdisError("close");

	return(0);
}


/********************************* PrintMdisError ***************************
 *
 *  Description: Print MDIS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintMdisError(char *info)
{
	printf("*** can't %s: %s\n", info, M_errstring(UOS_ErrnoGet()));
}

/********************************* PrintUosError ****************************
 *
 *  Description: Print UOS error message
 *
 *---------------------------------------------------------------------------
 *  Input......: info	info string
 *  Output.....: -
 *  Globals....: -
 ****************************************************************************/
static void PrintUosError(char *info)
{
	printf("*** can't %s: %s\n", info, UOS_ErrString(UOS_ErrnoGet()));
}

