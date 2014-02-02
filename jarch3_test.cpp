
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

/*============================================================*/
static void help() {
	fprintf(stderr,"jarch3 [options]\n");
	fprintf(stderr,"    -dev <device>\n");
	fprintf(stderr,"    -drv <driver>\n");
	fprintf(stderr,"    -c <command>\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    -no-mmap          Do not use memory-mapped I/O (device driver opt)\n");
	fprintf(stderr,"    -mmap             Use memory-mapped I/O (device driver opt)\n");
	fprintf(stderr,"    -s <sector>       When relevent to commands, what sector to operate on\n");
	fprintf(stderr,"    --page <n>        Mode sense page\n");
	fprintf(stderr,"    --subpage <n>     Mode sense subpage\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Commands:\n");
	fprintf(stderr,"    eject             Eject CD-ROM tray\n");		/* DONE */
	fprintf(stderr,"    retract           Retract CD-ROM tray\n");		/* DONE */
	fprintf(stderr,"    spinup            Spin up CD-ROM drive\n");		/* DONE */
	fprintf(stderr,"    spindown          Spin down CD-ROM drive\n");	/* DONE */
	fprintf(stderr,"    lock              Lock CD-ROM door\n");		/* DONE */
	fprintf(stderr,"    unlock            Unlock CD-ROM door\n");		/* DONE */
	fprintf(stderr,"    test-unit-ready   Test unit ready\n");		/* DONE */
	fprintf(stderr,"    read-subchannel   Read subchannel\n");		/* DONE */
	fprintf(stderr,"    seek              Seek the head\n");		/* DONE */
	fprintf(stderr,"    play-audio        Start CD playback\n");		/* DONE */
	fprintf(stderr,"    pause-audio       Pause CD playback\n");		/* DONE */
	fprintf(stderr,"    resume-audio      Pause CD playback\n");		/* DONE */
	fprintf(stderr,"    mode-sense        MODE SENSE\n");			/* DONE */
	fprintf(stderr,"    get-config        GET CONFIGURATION\n");		/* DONE */
	fprintf(stderr,"    get-profile       GET CONFIGURATION (profile only)\n"); /* DONE */
	fprintf(stderr,"    get-capacity      Get capacity\n");			/* DONE */
	fprintf(stderr,"    read-10           READ(10)\n");			/* DONE */
	fprintf(stderr,"    read-12           READ(12)\n");			/* DONE */
	fprintf(stderr,"    read-cd-data-mode1             READ CD (sector type mode 1)\n");	/* DONE */
	fprintf(stderr,"    read-cd-data-mode2-form1       READ CD (sector type mode 2 form 1)\n");	/* DONE */
	fprintf(stderr,"    read-cd-data-raw               READ CD (sector type any raw 2352)\n");	/* DONE */
	fprintf(stderr,"    read-msf-data-raw              READ MSF (sector type any raw 2352)\n");	/* DONE */
	fprintf(stderr,"\n");
	fprintf(stderr,"Driver: linux_sg\n");
	fprintf(stderr,"   Valid devices are of the form /dev/sr0, /dev/sr1, etc...\n");
}

static int parse_argv(Jarch3Configuration &cfg,int argc,char **argv) {
	char *a;
	int i;

	for (i=1;i < argc;) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"dev")) {
				if (i >= argc) return 1;
				cfg.device = argv[i++];
			}
			else if (!strcmp(a,"drv")) {
				if (i >= argc) return 1;
				cfg.driver = argv[i++];
			}
			else if (!strcmp(a,"c")) {
				if (i >= argc) return 1;
				cfg.command = argv[i++];
			}
			else if (!strcmp(a,"no-mmap")) {
				cfg.no_mmap = true;
			}
			else if (!strcmp(a,"mmap")) {
				cfg.no_mmap = false;
			}
			else if (!strcmp(a,"s")) {
				cfg.sector = strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"page")) {
				cfg.page = strtoul(argv[i++],NULL,0);
			}
			else if (!strcmp(a,"subpage")) {
				cfg.subpage = strtoul(argv[i++],NULL,0);
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				help();
				return 1;
			}
		}
		else {
			fprintf(stderr,"Unhandled arg %s\n",a);
			return 1;
		}
	}

	if (cfg.command == "" || cfg.command == "help") {
		help();
		return 1;
	}

	return 0;
}

int main(int argc,char **argv) {
	Jarch3Configuration config;
	Jarch3Driver *driver = NULL;
	Jarch3Device *device = NULL;

	if (parse_argv(config,argc,argv))
		return 1;

	/* locate and open the driver. this function will addref() for us */
	driver = Jarch3GetDriver(config.driver,config.device,&config);
	if (driver == NULL) {
		fprintf(stderr,"Unable to open driver %s device %s\n",config.driver.c_str(),config.device.c_str());
		return 1;
	}

	/* open the device */
	device = driver->open(config.device.c_str(),&config);
	if (device == NULL) {
		fprintf(stderr,"Failed to open device %s\n",config.device.c_str());
		driver->release();
		return 1;
	}

	fprintf(stderr,"Opened device %s (driver %s) successfully\n",config.device.c_str(),config.driver.c_str());
	fprintf(stderr,"    can_provide_residual:            %s\n",jarch3_yesno_str[device->can_provide_residual()?1:0]);
	fprintf(stderr,"    can_buffer_show_partial_reads:   %s\n",jarch3_yesno_str[device->can_buffer_show_partial_reads()?1:0]);
	fprintf(stderr,"    can_write_buffer:                %s\n",jarch3_yesno_str[device->can_write_buffer()?1:0]);

	if (config.command == "test-unit-ready") {
		if (jarch3_test_unit_ready(device)) printf("Test OK\n");
		else printf("Test failed\n");
	}
	else if (config.command == "eject") {
		if (jarch3_test_unit_ready(device,JARCH3_TUR_EXPECT_MEDIUM_NOT_PRESENT)) printf("Test unit ready OK\n");
		if (jarch3_start_stop_unit(device,0x02)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "retract") {
		if (jarch3_test_unit_ready(device,JARCH3_TUR_EXPECT_MEDIUM_NOT_PRESENT)) printf("Test unit ready OK\n");
		if (jarch3_start_stop_unit(device,0x03)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "spinup") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_start_stop_unit(device,0x01)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "spindown") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_start_stop_unit(device,0x00)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "lock") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_prevent_allow_medium_removal(device,0x01)) printf("PREVENT ALLOW MEDIUM REMOVAL OK\n");
	}
	else if (config.command == "unlock") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_prevent_allow_medium_removal(device,0x00)) printf("PREVENT ALLOW MEDIUM REMOVAL OK\n");
	}
	else if (config.command == "seek") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_seek_cdrom(device,config.sector)) printf("SEEK OK\n");
	}
	else if (config.command == "play-audio") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_play_audio(device,config.sector)) printf("PLAY AUDIO OK\n");
	}
	else if (config.command == "pause-audio") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_pause_resume_audio(device,/*resume=*/0)) printf("PAUSE OK\n");
	}
	else if (config.command == "resume-audio") {
		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if (jarch3_pause_resume_audio(device,/*resume=*/1)) printf("RESUME OK\n");
	}
	else if (config.command == "mode-sense") {
		unsigned char buffer[256];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_mode_sense(buffer,sizeof(buffer),device,config.page,config.subpage)) < 0) printf("RESUME OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");
	}
	else if (config.command == "read-subchannel") {
		unsigned char buffer[256];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_read_subchannel_curpos(buffer,sizeof(buffer),device,/*MSF=*/1,/*SUBQ=*/1)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		printf("Audio status: 0x%02x\n",buffer[1]);
		printf("Subchannel data length: %u\n",(buffer[2] << 8) + buffer[3]);
		printf("Subchannel format code: 0x%02x\n",buffer[4]);
		printf("ADR=%u CONTROL=%u\n",buffer[5]>>4,buffer[5]&0xF);
		printf("TRACK=%u\n",buffer[6]);
		printf("INDEX=%u\n",buffer[7]);
		printf("Absolute CD address M:S:F: %02u:%02u:%02u:%02u\n",buffer[8],buffer[9],buffer[10],buffer[11]);
		printf("Relative CD address M:S:F: %02u:%02u:%02u:%02u\n",buffer[12],buffer[13],buffer[14],buffer[15]);
	}
	else if (config.command == "get-profile") {
		unsigned char buffer[1024];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_get_configuration_profile_only(buffer,sizeof(buffer),device)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		/* NTS: Data Length field @0 is 32-bit wide and contains the length of data following the field.
		 *      Typically that means a total response of 424 bytes has a Data Length field of 420 */
		printf("Feature header: datalen=%u (when returned %u) current_profile=0x%04x %s\n",
			((unsigned int)buffer[0] << 24) + ((unsigned int)buffer[1] << 16) +
			((unsigned int)buffer[2] <<  8) + ((unsigned int)buffer[3]),
			rd,((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7]),
			jarch3_mmc_profile_to_str(((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7])));
	}
	else if (config.command == "get-config") {
		unsigned char buffer[16384],*s,*f;
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_get_configuration(buffer,sizeof(buffer),device)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		/* NTS: Data Length field @0 is 32-bit wide and contains the length of data following the field.
		 *      Typically that means a total response of 424 bytes has a Data Length field of 420 */
		printf("Feature header: datalen=%u (when returned %u) current_profile=0x%04x %s\n",
			((unsigned int)buffer[0] << 24) + ((unsigned int)buffer[1] << 16) +
			((unsigned int)buffer[2] <<  8) + ((unsigned int)buffer[3]),
			rd,((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7]),
			jarch3_mmc_profile_to_str(((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7])));

		s = buffer+8;
		f = buffer+rd;
		while ((s+4) <= f) {
			unsigned int fcode,len;

			len = s[3];
			fcode = ((unsigned int)s[0] << 8) + ((unsigned int)s[1]);
			printf("  Feature 0x%04x ver=%u persist=%u current=%u additional_length=%u",
				fcode,(s[2] >> 2) & 0xF,(s[2] >> 1) & 1,(s[2] >> 0) & 1,len);
			switch (fcode) {
				case 0x0000:	printf(" Profile List"); break;
				case 0x0001:	printf(" Core"); break;
				case 0x0002:	printf(" Morphing"); break;
				case 0x0003:	printf(" Removable Medium"); break;
				case 0x0004:	printf(" Write Protect"); break;
				case 0x0010:	printf(" Random Readable"); break;
				case 0x001D:	printf(" Multi-Read"); break;
				case 0x001E:	printf(" CD Read"); break;
				case 0x001F:	printf(" DVD Read"); break;
				case 0x0020:	printf(" Random Writeable"); break;
				case 0x0021:	printf(" Incremental Streaming Writeable"); break;
				case 0x0022:	printf(" Legacy (Sector Eraseable Feature)"); break;
				case 0x0023:	printf(" Formattable"); break;
				case 0x0024:	printf(" Hardware Defect Management"); break;
				case 0x0025:	printf(" Write Once"); break;
				case 0x0026:	printf(" Restricted Overwrite"); break;
				case 0x0027:	printf(" CD-RW CAV Write"); break;
				case 0x0028:	printf(" MRW"); break;
				case 0x0029:	printf(" Enhanced Defect Reporting"); break;
				case 0x002A:	printf(" DVD+RW"); break;
				case 0x002B:	printf(" DVD+R"); break;
				case 0x002C:	printf(" Rigid Restricted Overwrite"); break;
				case 0x002D:	printf(" CD Track at Once"); break;
				case 0x002E:	printf(" CD Mastering"); break;
				case 0x002F:	printf(" DVD-R/-RW Write"); break;
				case 0x0033:	printf(" Layer Jump Recording"); break;
				case 0x0034:	printf(" LJ Rigid Restricted Overwrite"); break;
				/* TODO: Copy down more values from MMC-6 document */
			};
			printf("\n");
			s += 4;
			if ((s+len) > f) break;

			s += len;
		}
	}
	else if (config.command == "get-capacity") {
		unsigned char buffer[256];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_get_capacity(buffer,sizeof(buffer),device)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		printf("LBA=%u BlockLength=%u\n",
			((unsigned int)buffer[0] << 24) + ((unsigned int)buffer[1] << 16) + 
			((unsigned int)buffer[2] <<  8) + ((unsigned int)buffer[3] <<  0),
			((unsigned int)buffer[4] << 24) + ((unsigned int)buffer[5] << 16) + 
			((unsigned int)buffer[6] <<  8) + ((unsigned int)buffer[7] <<  0));
	}
	else if (config.command == "read-10") {
		unsigned char buffer[2048];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_read10(buffer,sizeof(buffer),device,config.sector,1)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		for (i=0;i < rd;i++) {
			if (buffer[i] >= 32 && buffer[i] < 127) printf("%c",buffer[i]);
			else printf(".");
		}
		printf("\n");
	}
	else if (config.command == "read-12") {
		unsigned char buffer[2048];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_read12(buffer,sizeof(buffer),device,config.sector,1)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		for (i=0;i < rd;i++) {
			if (buffer[i] >= 32 && buffer[i] < 127) printf("%c",buffer[i]);
			else printf(".");
		}
		printf("\n");
	}
	else if (config.command == "read-cd-data-mode1") {
		unsigned char buffer[2048];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_readcd(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=MODE-1*/2,/*dap=*/0,/*b9=*/0x10,/*b10=*/0x00)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		for (i=0;i < rd;i++) {
			if (buffer[i] >= 32 && buffer[i] < 127) printf("%c",buffer[i]);
			else printf(".");
		}
		printf("\n");
	}
	else if (config.command == "read-cd-data-mode2-form1") {
		unsigned char buffer[2048];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_readcd(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=MODE-2 FORM-1*/4,/*dap=*/0,/*b9=*/0x10,/*b10=*/0x00)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		for (i=0;i < rd;i++) {
			if (buffer[i] >= 32 && buffer[i] < 127) printf("%c",buffer[i]);
			else printf(".");
		}
		printf("\n");
	}
	else if (config.command == "read-cd-data-raw") {
		unsigned char buffer[2352];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_readcd(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=any*/0,/*dap=*/0,/*b9=*/0xF8,/*b10=*/0x00)) < 0) printf(" OK\n");
		printf("Got %u bytes\n",rd);

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		for (i=0;i < rd;i++) {
			if (buffer[i] >= 32 && buffer[i] < 127) printf("%c",buffer[i]);
			else printf(".");
		}
		printf("\n");
	}
	else if (config.command == "read-msf-data-raw") {
		unsigned char buffer[2352*2];
		int rd,i;

		if (jarch3_test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=jarch3_readmsf(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=any*/0,/*dap=*/0,/*b9=*/0xF8,/*b10=*/0x00)) < 0) printf(" OK\n");
		printf("Got %u bytes\n",rd);

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		for (i=0;i < rd;i++) {
			if (buffer[i] >= 32 && buffer[i] < 127) printf("%c",buffer[i]);
			else printf(".");
		}
		printf("\n");
	}

	/* we're finished with the device */
	device->release(); device = NULL;

	/* we're finished with the driver */
	driver->release(); driver = NULL;
	return 0;
}

