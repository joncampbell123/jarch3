
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

/*===================== empty base device ==================*/
Jarch3Device::Jarch3Device(const char *dev,Jarch3Driver *drv,Jarch3Configuration UNUSED *cfg) {
	timeout = -1;
	cmd_length = 0;
	data_length = 0;
	sense_length = 0;
	no_mmap = cfg->no_mmap;
	auto_fetch_sense = true;
	driver = drv;
	driver->addref();
	driver->dev_refcount++;
	refcount = 0;
	device = dev;
	fd = -1;
}

Jarch3Device::~Jarch3Device() {
	close();
	driver->dev_refcount--;
	driver->release();
}

void Jarch3Device::close() {
	if (fd >= 0) ::close(fd);
	fd = -1;
}

bool Jarch3Device::open() {
	return false;
}

int Jarch3Device::addref() {
	return ++refcount;
}

int Jarch3Device::release() {
	int ret;

	assert(refcount > 0);
	ret = --refcount;
	if (ret == 0) delete this;
	return ret;
}

bool Jarch3Device::can_provide_residual() {
	return false;
}

bool Jarch3Device::can_buffer_show_partial_reads() {
	return false;
}

bool Jarch3Device::can_write_buffer() {
	return false;
}

unsigned char* Jarch3Device::read_buffer(size_t UNUSED len) {
	return NULL;
}

size_t Jarch3Device::read_buffer_length() {
	return 0;
}

unsigned char* Jarch3Device::write_command(size_t UNUSED len) {
	if (len > sizeof(cmd)) return NULL;
	cmd_length = len;
	return cmd;
}

size_t Jarch3Device::write_command_length() {
	return cmd_length;
}

unsigned char* Jarch3Device::read_sense(size_t UNUSED len) {
	if (sense_length == 0 || len > sense_length) return NULL;
	return sense;
}

size_t Jarch3Device::read_sense_length() {
	return sense_length;
}

void Jarch3Device::clear_data() {
	data_length = 0;
}

void Jarch3Device::clear_sense() {
	sense_length = 0;
}

void Jarch3Device::clear_command() {
	cmd_length = 0;
}

size_t Jarch3Device::read_buffer_data_length() {
	return data_length;
}

void Jarch3Device::dump_sense(FILE *fp) {
	int x;

	if (fp == NULL) fp = stderr;
	if (sense_length == 0) {
		fprintf(fp,"No sense data\n");
		return;
	}

	fprintf(fp,"Sense data[%u]: ",(unsigned int)sense_length);
	for (x=0;x < (int)sense_length;x++) {
		if ((x&3) == 3) fprintf(fp," ");
		fprintf(fp,"%02x ",sense[x]);
	}
	fprintf(fp,"\n");

	if (sense_length > 2) {
		fprintf(fp,"     Which means: Key=%s  ASC=%s\n",
			jarch3_scsi_keys[sense[2]&0xF],
			jarch3_scsi_asc(sense[2]&0xF,sense[12],sense[13]));
	}
}

int Jarch3Device::do_scsi(int UNUSED direction,size_t UNUSED n_data_length) {
	errno = ENOSYS;
	return -1;
}

