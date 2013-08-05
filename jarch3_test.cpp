
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <scsi/sg.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include <string>

#define UNUSED __attribute__((unused))

using namespace std;

class Jarch3Configuration {
public:
				Jarch3Configuration();
				~Jarch3Configuration();
	void			reset();
public:
	string			driver;
	string			device;
	string			command;
};

class Jarch3Device;

class Jarch3Driver {
public:
				Jarch3Driver();
	virtual			~Jarch3Driver();
public:
	virtual bool		init();
	virtual Jarch3Device*	open(const char *dev,Jarch3Configuration *cfg);
	virtual void		close();
public:
	virtual int		addref();
	virtual int		release();	/* NTS: upon refcount == 0, will auto-delete itself */
	volatile int		refcount;
	volatile int		dev_refcount;
};

class Jarch3Driver_Linux_SG : public Jarch3Driver {
public:
				Jarch3Driver_Linux_SG();
	virtual			~Jarch3Driver_Linux_SG();
public:
	virtual bool		init();
	virtual Jarch3Device*	open(const char *dev,Jarch3Configuration *cfg);
};

class Jarch3Device {
public:
				Jarch3Device(const char *dev,Jarch3Driver *drv);
	virtual			~Jarch3Device();
public:
	virtual void		close();
	virtual bool		open();
public:
	int			fd;
	string			device;
	Jarch3Driver*		driver;
public:
	virtual int		addref();
	virtual int		release();	/* NTS: upon refcount == 0, will auto-delete itself */
	volatile int		refcount;
};

class Jarch3Device_Linux_SG : public Jarch3Device {
public:
				Jarch3Device_Linux_SG(const char *dev,Jarch3Driver *drv);
	virtual			~Jarch3Device_Linux_SG();
public:
	virtual bool		open();
};

/*===================== config class ==================*/
Jarch3Configuration::Jarch3Configuration() {
	reset();
}

Jarch3Configuration::~Jarch3Configuration() {
}

void Jarch3Configuration::reset() {
	driver = "linux_sg";
	device = "/dev/dvd";
	command.clear();
}

/*===================== empty base device ==================*/
Jarch3Device::Jarch3Device(const char *dev,Jarch3Driver *drv) {
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

	devobj = new Jarch3Device_Linux_SG(dev,this);
	if (devobj == NULL) return NULL;
	devobj->addref();

	if (!devobj->open()) {
		fprintf(stderr,"Linux_SG: %s failed to open\n",dev);
		devobj->release();
		delete devobj;
		return NULL;
	}

	return devobj;
}

/*====================== Linux SGIO device ===================*/
Jarch3Device_Linux_SG::Jarch3Device_Linux_SG(const char *dev,Jarch3Driver *drv) : Jarch3Device(dev,drv) {
}

Jarch3Device_Linux_SG::~Jarch3Device_Linux_SG() {
}

bool Jarch3Device_Linux_SG::open() {
	if (fd >= 0) return true;

	fd = ::open(device.c_str(),O_RDWR | O_LARGEFILE | O_NONBLOCK | O_EXCL);
	if (fd < 0) {
		fprintf(stderr,"Linux_SG: Failed to open %s, %s\n",device.c_str(),strerror(errno));
		return false;
	}

	return true;
}

/*===================== Driver dispatch ======================*/
/* given driver name and device, locate/load driver, addref, and return */
Jarch3Driver *Jarch3GetDriver(string &driver,string UNUSED &device,Jarch3Configuration UNUSED *cfg) {
	Jarch3Driver *ret = NULL;

	if (driver == "linux_sg")
		ret = new Jarch3Driver_Linux_SG();

	if (ret != NULL) {
		if (ret->addref() == 1) {
			if (!ret->init()) {
				ret->release();
				ret = NULL;
			}
		}
	}

	return ret;
}

/*============================================================*/
static void help() {
	fprintf(stderr,"jarch3 [options]\n");
	fprintf(stderr,"    -dev <device>\n");
	fprintf(stderr,"    -drv <driver>\n");
	fprintf(stderr,"    -c <command>\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Commands:\n");
	fprintf(stderr,"    eject             Eject CD-ROM tray\n");
	fprintf(stderr,"    retract           Retract CD-ROM tray\n");
	fprintf(stderr,"    spinup            Spin up CD-ROM drive\n");
	fprintf(stderr,"    spindown          Spin down CD-ROM drive\n");
	fprintf(stderr,"    lock              Lock CD-ROM door\n");
	fprintf(stderr,"    unlock            Unlock CD-ROM door\n");
	fprintf(stderr,"    test-unit-ready   Test unit ready\n");
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
		return 1;
	}

	/* we're finished with the device */
	device->release(); device = NULL;

	/* we're finished with the driver */
	driver->release(); driver = NULL;
	return 0;
}

