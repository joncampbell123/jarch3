
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

/*===================== empty base driver ==================*/
Jarch3Driver::Jarch3Driver() {
	dev_refcount = 0;
	refcount = 0;
}

Jarch3Driver::~Jarch3Driver() {
	if (dev_refcount != 0) fprintf(stderr,"WARNING: Jarch3Driver destructor with %d device references active!\n",dev_refcount);
	close();
}

bool Jarch3Driver::init() {
	return false;
}

Jarch3Device *Jarch3Driver::open(const char UNUSED *dev,Jarch3Configuration UNUSED *cfg) {
	return NULL;
}

void Jarch3Driver::close() {
}

int Jarch3Driver::addref() {
	return ++refcount;
}

int Jarch3Driver::release() {
	int ret;

	assert(refcount > 0);
	ret = --refcount;
	if (ret == 0) delete this;
	return ret;
}

/*===================== Driver dispatch ======================*/
/* given driver name and device, locate/load driver, addref, and return */
Jarch3Driver *Jarch3GetDriver(string &driver,string UNUSED &device,Jarch3Configuration UNUSED *cfg) {
	Jarch3Driver *ret = NULL;

	if (driver == "linux_sg")
		ret = new Jarch3Driver_Linux_SG();

	if (ret != NULL) {
		ret->addref();
		if (!ret->init()) {
			ret->release();
			ret = NULL;
		}
	}

	return ret;
}

