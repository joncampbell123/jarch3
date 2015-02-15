
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

/*===================== Linux SGIO driver ==================*/
Jarch3Driver_Linux_SG::Jarch3Driver_Linux_SG() : Jarch3Driver() {
}

Jarch3Driver_Linux_SG::~Jarch3Driver_Linux_SG() {
}

bool Jarch3Driver_Linux_SG::init() {
	return true;
}

Jarch3Device *Jarch3Driver_Linux_SG::open(const char *dev,Jarch3Configuration UNUSED *cfg) {
	Jarch3Device *devobj;
	struct stat st;

	if (stat(dev,&st)) {
		fprintf(stderr,"Linux_SG: Cannot stat %s, %s\n",dev,strerror(errno));
		return NULL;
	}
	if (!S_ISBLK(st.st_mode)) {
		fprintf(stderr,"Linux_SG: %s is not block device\n",dev);
		return NULL;
	}

	devobj = new Jarch3Device_Linux_SG(dev,this,cfg);
	if (devobj == NULL) return NULL;
	devobj->addref();

	if (!devobj->open()) {
		fprintf(stderr,"Linux_SG: %s failed to open\n",dev);
		devobj->release();
		return NULL;
	}

	return devobj;
}

/*====================== Linux SGIO device ===================*/
Jarch3Device_Linux_SG::Jarch3Device_Linux_SG(const char *dev,Jarch3Driver *drv,Jarch3Configuration UNUSED *cfg) : Jarch3Device(dev,drv,cfg) {
	reserved_mmap = NULL;
	user_buffer = NULL;
	reserved_size = 0;
}

Jarch3Device_Linux_SG::~Jarch3Device_Linux_SG() {
}

void Jarch3Device_Linux_SG::close() {
	if (user_buffer != NULL) free(user_buffer);
	if (reserved_mmap != NULL) munmap(reserved_mmap,reserved_size);
	Jarch3Device::close();
}

bool Jarch3Device_Linux_SG::open() {
	if (fd >= 0) return true;

	fd = ::open(device.c_str(),O_RDWR | O_LARGEFILE | O_NONBLOCK | O_EXCL);
	if (fd < 0) {
		fprintf(stderr,"Linux_SG: Failed to open %s, %s\n",device.c_str(),strerror(errno));
		return false;
	}

	reserved_size = 65536;
	ioctl(fd,SG_SET_RESERVED_SIZE,&reserved_size);
	ioctl(fd,SG_GET_RESERVED_SIZE,&reserved_size);
	fprintf(stderr,"Linux_SG: Reserved size set to %u\n",reserved_size);

	if (no_mmap) {
		fprintf(stderr,"Linux_SG: Not using memory-mapping\n");
		reserved_mmap = NULL;
	}
	else {
		reserved_mmap = mmap(NULL,reserved_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
		if (reserved_mmap == MAP_FAILED) {
			fprintf(stderr,"Linux_SG: Unable to mmap the reserve.\n");
			reserved_mmap = NULL;
		}
		else {
			fprintf(stderr,"Linux_SG: Successfully memory-mapped the SGIO reserve buffer\n");
		}
	}

	if (reserved_mmap == NULL) {
		reserved_size = 0x10000;
		user_buffer = malloc(reserved_size);
		if (user_buffer == NULL) fprintf(stderr,"Linux_SG: Unable to alloc user buffer\n");
	}

	return true;
}

bool Jarch3Device_Linux_SG::can_provide_residual() {
	return true;
}

bool Jarch3Device_Linux_SG::can_buffer_show_partial_reads() {
	return true;
}

bool Jarch3Device_Linux_SG::can_write_buffer() {
	return (reserved_mmap != NULL || user_buffer != NULL)?true:false; /* yes, IF we are using mmap I/O */
}

unsigned char* Jarch3Device_Linux_SG::read_buffer(size_t len) {
	if (len > (size_t)reserved_size) return NULL;
	if (reserved_mmap != NULL) return (unsigned char*)reserved_mmap;
	return (unsigned char*)user_buffer;
}

size_t Jarch3Device_Linux_SG::read_buffer_length() {
	return reserved_size;
}

int Jarch3Device_Linux_SG::do_scsi(int direction,size_t n_data_length) {
	struct sg_io_hdr sg;
	int r;

	if (fd < 0) {
		errno = EBADF;
		return -1;
	}
	if (cmd_length == 0) {
		errno = EINVAL;
		return -1;
	}

	memset(&sg,0,sizeof(sg));
	memset(sense,0,sizeof(sense));
	data_length = n_data_length;
	sense_length = 0;

	sg.interface_id =			'S';
	sg.cmd_len =				cmd_length;
	sg.cmdp =				cmd;
	sg.mx_sb_len =				sizeof(sense);
	sg.sbp =				sense;
	sg.dxfer_len =				(direction != DirNone) ? n_data_length : 0;
	sg.dxferp =				(direction != DirNone) ? (reserved_mmap != NULL ? reserved_mmap : user_buffer) : NULL;
	sg.timeout =				(timeout >= 0) ? timeout : 30000;
	sg.flags =				(reserved_mmap != NULL ? SG_FLAG_MMAP_IO : SG_FLAG_DIRECT_IO);
	if (direction == DirToHost)		sg.dxfer_direction = SG_DXFER_FROM_DEV;
	else if (direction == DirToDevice)	sg.dxfer_direction = SG_DXFER_TO_DEV;
	else					sg.dxfer_direction = SG_DXFER_NONE;

	if (n_data_length != 0 && direction != DirNone) {
		if (sg.dxferp == NULL && reserved_mmap == NULL) {
			fprintf(stderr,"do_scsi() attempt to do data command with no buffer\n");
			errno = EINVAL;
			return -1;
		}
		else if ((size_t)sg.dxfer_len > (size_t)reserved_size) {
			fprintf(stderr,"do_scsi() attempt to do data command with transfer larger than buffer\n");
			errno = EINVAL;
			return -1;
		}
	}

	r = ioctl(fd,SG_IO,(void*)(&sg));
	if (r < 0) fprintf(stderr,"Linux_SG: SG_IO failure %s\n",strerror(errno));

	sense_length = sg.sb_len_wr;
	if (sg.driver_status != 0) {
		if (r == 0) { r = -1; errno = EIO; }
	}
	if (sg.masked_status != 0) {
		if (r == 0) { r = -1; errno = EIO; }
	}
	if (sg.host_status != 0) {
		if (r == 0) { r = -1; errno = EIO; }
	}

	if ((size_t)sg.resid > (size_t)sg.dxfer_len) {
		fprintf(stderr,"Linux_SG: SG_IO residual > transfer length\n");
		sg.resid = sg.dxfer_len;
	}
	data_length = sg.dxfer_len - sg.resid;

	/* NTS: If a command is successful, the Linux SG_IO ioctl will NOT return sense data */
	if (sense_length == 0 && auto_fetch_sense && r != 0) {
		static const unsigned char sense_cmd[6] = {0x03,0x00,0x00,0x00,(unsigned char)sizeof(sense),(unsigned char)(sizeof(sense)>>8)};

		fprintf(stderr,"Linux_SG: SG_IO did not provide sense. Requesting it now\n");

		memset(&sg,0,sizeof(sg));
		sg.interface_id =			'S';
		sg.cmd_len =				sizeof(sense_cmd);
		sg.cmdp =				(unsigned char*)sense_cmd;
		sg.timeout =				30000;
		sg.dxferp =				sense;
		sg.dxfer_len =				sizeof(sense);
		sg.dxfer_direction =			SG_DXFER_FROM_DEV;
		sg.flags =				SG_FLAG_DIRECT_IO;

		if (ioctl(fd,SG_IO,(void*)(&sg)) < 0) {
			fprintf(stderr,"Linux_SG: SG_IO did not provide sense even when explicitly asked\n");
		}
		else if (sg.resid != 0) {
			sense_length = sg.dxfer_len - sg.resid;
		}
		else {
			sense_length = 7 + sense[7]; /* <- Is this right? */
			if (sense_length > sizeof(sense)) sense_length = sizeof(sense);
		}
	}

	return r;
}

