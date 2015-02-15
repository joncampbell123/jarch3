
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

bool jarch3_test_unit_ready(Jarch3Device *dev,unsigned int expect) {
	unsigned char *p;
	int last_rep=0;
	int r;

	do {
		p = dev->write_command(6);	/* TEST UNIT READY is 6 bytes long */
		if (p == NULL) {
			fprintf(stderr,"%s: cannot obtain command buffer\n",__func__);
			return false;
		}

		memset(p,0,6);		/* set all to zero. TEST UNIT READY command byte is also zero */
		r = dev->do_scsi();
		if (r < 0) {
			p = dev->read_sense(14);
			if (p != NULL) {
				if ((p[2]&0xF) == 2 && p[12] == 0x3A) {
					if (expect&JARCH3_TUR_EXPECT_MEDIUM_NOT_PRESENT)
						break;

					if (last_rep != 0x023A00) {
						last_rep = 0x023A00;
						fprintf(stderr,"Device reports medium not present. Insert media or CTRL+C now\n");
					}
					sleep(1);
					continue;
				}
				if ((p[2]&0xF) == 2 && p[12] == 0x04 && p[13] == 0x01) {
					if (last_rep != 0x020401) {
						last_rep = 0x020401;
						fprintf(stderr,"Device reports medium becoming available.\n");
					}
					usleep(250000);
					continue;
				}
				if ((p[2]&0xF) == 6 && p[12] == 0x28 && p[13] == 0x00) {
					if (last_rep != 0x062800) {
						last_rep = 0x062800;
						fprintf(stderr,"Device reports medium is ready.\n");
					}
					usleep(250000);
					continue;
				}
			}

			fprintf(stderr,"%s: do_scsi() failed, %s\n",__func__,strerror(errno));
			dev->dump_sense(stderr);
			return false;
		}
		
		break;
	} while (1);

	return true;
}

bool jarch3_start_stop_unit(Jarch3Device *dev,unsigned char ctl/*[1:0] = LoEj, Start*/) {
	unsigned char *p;

	p = dev->write_command(6);
	if (p != NULL) {
		p[0] = 0x1B;		/* START STOP UNIT */
		p[4] = ctl;		/* LoEj, Start */
		if (dev->do_scsi() < 0) {
			printf("START STOP UNIT failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		return true;
	}

	return false;
}

bool jarch3_prevent_allow_medium_removal(Jarch3Device *dev,unsigned char ctl) {
	unsigned char *p;

	p = dev->write_command(6);
	if (p != NULL) {
		p[0] = 0x1E;		/* PREVENT ALLOW MEDIUM REMOVAL */
		p[4] = ctl;		/* LoEj, Start */
		if (dev->do_scsi() < 0) {
			printf("PREVENT ALLOW MEDIUM REMOVAL failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		return true;
	}

	return false;
}

bool jarch3_seek_cdrom(Jarch3Device *dev,unsigned long sector) {
	unsigned char *p;

	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x2B;		/* SEEK */
		p[2] = sector >> 24;
		p[3] = sector >> 16;
		p[4] = sector >> 8;
		p[5] = sector;
		if (dev->do_scsi() < 0) {
			printf("SEEK failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		return true;
	}

	return false;
}

bool jarch3_pause_resume_audio(Jarch3Device *dev,unsigned char resume) {
	unsigned char *p;

	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x4B;		/* PAUSE/RESUME */
		p[8] = resume ? 0x01 : 0x00;
		if (dev->do_scsi() < 0) {
			printf("PLAY AUDIO failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		return true;
	}

	return false;
}

bool jarch3_play_audio(Jarch3Device *dev,unsigned long sector) {
	unsigned char *p;

	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x45;		/* PLAY AUDIO */
		p[2] = sector >> 24;
		p[3] = sector >> 16;
		p[4] = sector >> 8;
		p[5] = sector;
		p[7] = 0xFF;
		p[8] = 0xFF;
		if (dev->do_scsi() < 0) {
			printf("PLAY AUDIO failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		return true;
	}

	return false;
}

int jarch3_read10(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x28;		/* READ(10) */
		p[1] = 0x00;		/* RT=0 */
		p[2] = (lba >> 24);
		p[3] = (lba >> 16);
		p[4] = (lba >> 8);
		p[5] = lba;
		p[6] = 0;
		p[7] = sects >> 8;
		p[8] = sects;
		p[9] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("READ(10) failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_read12(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(12);
	if (p != NULL) {
		p[0] = 0xA8;		/* READ(12) */
		p[1] = 0x00;		/* RT=0 */
		p[2] = (lba >> 24);
		p[3] = (lba >> 16);
		p[4] = (lba >> 8);
		p[5] = lba;
		p[6] = sects >> 24;
		p[7] = sects >> 16;
		p[8] = sects >> 8;
		p[9] = sects;
		p[10] = 0;
		p[11] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("READ(12) failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_readcd(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects,unsigned char expected_sector_type,unsigned char dap,unsigned char b9,unsigned char b10) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(12);
	if (p != NULL) {
		p[0] = 0xBE;		/* READ CD */
		p[1] = (expected_sector_type << 2) | (dap << 1);
		p[2] = (lba >> 24);
		p[3] = (lba >> 16);
		p[4] = (lba >> 8);
		p[5] = lba;
		p[6] = sects >> 16;
		p[7] = sects >> 8;
		p[8] = sects;
		p[9] = b9;
		p[10] = b10;
		p[11] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("READ CD failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_readmsf(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects,unsigned char expected_sector_type,unsigned char dap,unsigned char b9,unsigned char b10) {
	unsigned char SM,SS,SF,EM,ES,EF;
	unsigned char *p,*s;
	size_t l;

	/* NTS: CD reads starting at M:S:F start, stops at M:S:F end, does not include M:S:F end.
	 *      So to read 1 sector at 0:2:0 you would have start=0:2:0 end=0:2:1 */
	lba += 150;
	SF = (lba % 75UL);
	SS = (lba / 75UL) % 60UL;
	SM = (lba / 75UL) / 60UL;
	lba += sects;
	EF = (lba % 75UL);
	ES = (lba / 75UL) % 60UL;
	EM = (lba / 75UL) / 60UL;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(12);
	if (p != NULL) {
		p[0] = 0xB9;		/* READ CD MSF */
		p[1] = (expected_sector_type << 2) | (dap << 1);
		p[2] = 0;
		p[3] = SM;
		p[4] = SS;
		p[5] = SF;
		p[6] = EM;
		p[7] = ES;
		p[8] = EF;
		p[9] = b9;
		p[10] = b10;
		p[11] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("READ CD failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_get_capacity(void *dst,size_t dstmax,Jarch3Device *dev) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x25;		/* READ CAPACITY */
		p[1] = 0x00;		/* RT=0 */
		p[2] = 0x00;		/* Starting feature number==0 */
		p[3] = 0x00;		/* ditto */
		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = 0;
		p[8] = 0;
		p[9] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("GET CONFIGURATION failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_get_configuration(void *dst,size_t dstmax,Jarch3Device *dev) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x46;		/* GET CONFIGURATION */
		p[1] = 0x00;		/* RT=0 */
		p[2] = 0x00;		/* Starting feature number==0 */
		p[3] = 0x00;		/* ditto */
		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = dstmax >> 8;
		p[8] = dstmax;
		p[9] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("GET CONFIGURATION failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_get_configuration_profile_only(void *dst,size_t dstmax,Jarch3Device *dev) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x46;		/* GET CONFIGURATION */
		p[1] = 0x02;		/* RT=2 feature header and only the feature descriptor asked for */
		p[2] = 0xFF;		/* Starting feature number==0xFFFF (i.e. one that doesn't exist) */
		p[3] = 0xFF;		/* ditto */
		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = dstmax >> 8;
		p[8] = dstmax;
		p[9] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("GET CONFIGURATION failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_read_subchannel_curpos(void *dst,size_t dstmax,Jarch3Device *dev,unsigned char MSF,unsigned char SUBQ) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(10);
	if (p != NULL) {
		size_t max = 4 + (SUBQ?12:0);

		if (dstmax > max) dstmax = max;

		p[0] = 0x42;		/* READ SUB-CHANNEL */
		p[1] = (MSF?0x02:0x00);
		p[2] = (SUBQ?0x40:0x00);
		p[3] = 0x01;		/* read current CD position */
		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = dstmax >> 8;
		p[8] = dstmax;
		p[9] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("READ SUB-CHANNEL failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

int jarch3_mode_sense(void *dst,size_t dstmax,Jarch3Device *dev,unsigned char PAGE,unsigned char SUBPAGE) {
	unsigned char *p,*s;
	size_t l;

	dev->clear_data();
	dev->clear_sense();
	dev->clear_command();
	p = dev->write_command(10);
	if (p != NULL) {
		p[0] = 0x5A;		/* MODE SENSE */
		p[1] = 0x00;
		p[2] = PAGE & 0x3F;
		p[3] = SUBPAGE;
		p[4] = 0;
		p[5] = 0;
		p[6] = 0;
		p[7] = dstmax >> 8;
		p[8] = dstmax;
		p[9] = 0;
		if (dev->do_scsi(Jarch3Device::DirToHost,dstmax) < 0) {
			printf("MODE SENSE failed\n");
			dev->dump_sense(stdout);
			return false;
		}

		l = dev->read_buffer_data_length();
		if (l == 0) return 0;
		if (l > dstmax) return -1;
		s = dev->read_buffer(l);
		if (s == NULL) return -1;
		memcpy(dst,s,l);
		return (int)l;
	}

	return -1;
}

