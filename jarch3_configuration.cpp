
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

/*===================== config class ==================*/
Jarch3Configuration::Jarch3Configuration() {
	reset();
}

Jarch3Configuration::~Jarch3Configuration() {
}

void Jarch3Configuration::reset() {
	page = 0;
	sector = 0;
	subpage = 0;
	driver = "linux_sg";
	device = "/dev/dvd";
	command.clear();
	no_mmap = false;
}

