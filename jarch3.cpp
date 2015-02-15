
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <scsi/sg.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include <string>

#include "jarch3/jarch3.h"

using namespace std;

const char *jarch3_yesno_str[2] = {"No","Yes"};

const char *jarch3_scsi_keys[16] = {
	"No Sense",		/* 0x0 */
	"Soft Error",		/* 0x1 */
	"Not Ready",		/* 0x2 */
	"Medium Error",		/* 0x3 */

	"Hardware Error",	/* 0x4 */
	"Illegal Request",	/* 0x5 */
	"Unit Attention",	/* 0x6 */
	"Write Protect",	/* 0x7 */

	"",			/* 0x8 */
	"",			/* 0x9 */
	"",			/* 0xA */
	"Aborted Command",	/* 0xB */

	"",			/* 0xC */
	"",			/* 0xD */
	"",			/* 0xE */
	""			/* 0xF */
};

const char *jarch3_scsi_asc(unsigned char key,unsigned char asc,unsigned char ascq) {
	switch (key) {
		case 0x0:
			switch (asc) {
				case 0x00:	return "No error";
				case 0x5D:	return "No sense - PFA threshold reached";
			}
			break;
		case 0x1:
			switch ((asc<<8)+ascq) {
				case 0x0100:	return "Recovered Write error - no index";
				case 0x0200:	return "Recovered no seek completion";
				/* TODO: The rest */
			}
			break;
		case 0x2:
			switch ((asc<<8)+ascq) {
				case 0x0400:	return "Not Ready - Cause not reportable.";
				case 0x0401:	return "Not Ready - becoming ready";
				case 0x0402:	return "Not Ready - need initialise command (start unit)";
				/* TODO: The rest */
			}
			break;
		case 0x5:
			switch ((asc<<8)+ascq) {
				case 0x5302:	return "Illegal Request - medium removal prevented";
				/* TODO: The rest */
			}
			break;

		default:
			break;
	};

	return "?";
}

const char *jarch3_mmc_profile_to_str(unsigned int p) {
	switch (p) {
		case 0x0000:	return "No Current Profile";
		case 0x0002:	return "Removable Disk Profile";
		case 0x0008:	return "CD-ROM Profile";
		case 0x0009:	return "CD-R Profile";
		case 0x000A:	return "CD-RW Profile";
		case 0x0010:	return "DVD-ROM Profile";
		case 0x0011:	return "DVD-R Sequential Recording Profile";
		case 0x0012:	return "DVD-RAM Profile";
		case 0x0013:	return "DVD-RW Restricted Overwrite Profile";
		case 0x0014:	return "DVD-RW Sequential Recording Profile";
		case 0x0015:	return "DVD-R Dual Layer Sequential Recording Profile";
		case 0x0016:	return "DVD-R Dual Layer Jump Recording Profile";
		case 0x0017:	return "DVD-RW Dual Layer Profile";
		case 0x0018:	return "DVD-Download Disc Recording Profile";
		case 0x001A:	return "DVD+RW Profile";
		case 0x001B:	return "DVD+R Profile";
		case 0x002A:	return "DVD+RW Dual Layer Profile";
		case 0x002B:	return "DVD+R Dual Layer Profile";
		case 0x0040:	return "BD-ROM Profile";
		case 0x0041:	return "BD-R Sequential Recording Profile";
		/* TODO: More from MMC-6 */
	};

	return NULL;
}

