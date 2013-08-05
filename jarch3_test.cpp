
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
public:
	bool			no_mmap;
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
	enum {
		DirNone=0,
		DirToHost,		/* aka "reading" */
		DirToDevice		/* aka "writing" */
	};
public:
				Jarch3Device(const char *dev,Jarch3Driver *drv,Jarch3Configuration UNUSED *cfg);
	virtual			~Jarch3Device();
public:
	virtual void		close();
	virtual bool		open();

	virtual bool		can_provide_residual();			/* does the device provide a residual
									   byte count i.e. how much LESS was
									   returned compared to the full request.

									   REASON: some parts of this code would
									   like to know if bad sector reads returned
									   any data, even if not the entire sector. */

	virtual bool		can_buffer_show_partial_reads();	/* if a read was partial, not full,
									   would the buffer reflect the partial
									   contents on top of what was last there?
									   or would the contents change entirely?
									   
									   REASON: Knowing that partial reads will
									   leave behind the prior buffer contents
									   is useful for detecting what exactly was
									   read in case the driver does not provide
									   residual byte count */

	virtual bool		can_write_buffer();			/* does the driver allow us to write the
									   buffer, and have the written contents
									   remain where I/O does not overwrite?
									   
									   REASON: Some code might attempt to detect
									   residual byte code (if not provided by
									   the driver) by zeroing the buffer then
									   issuing a read, and then assuming that
									   any nonzero portion of the buffer represents
									   the actual residual data. */

	virtual unsigned char*	read_buffer(size_t len);
	virtual size_t		read_buffer_length();
	virtual size_t		read_buffer_data_length();
	virtual unsigned char*	write_command(size_t len);
	virtual size_t		write_command_length();
	virtual unsigned char*	read_sense(size_t len);
	virtual size_t		read_sense_length();
	virtual void		clear_command();
	virtual void		clear_sense();
	virtual void		clear_data();
	virtual int		do_scsi(int direction=0,size_t data_length=0);
public:
	int			fd;
	string			device;
	Jarch3Driver*		driver;

	unsigned char		sense[256];
	size_t			sense_length;

	unsigned char		cmd[256];
	size_t			cmd_length;

	int			timeout;
	size_t			data_length;
public:
	virtual int		addref();
	virtual int		release();	/* NTS: upon refcount == 0, will auto-delete itself */
	volatile int		refcount;
public:
	bool			no_mmap;
};

class Jarch3Device_Linux_SG : public Jarch3Device {
public:
				Jarch3Device_Linux_SG(const char *dev,Jarch3Driver *drv,Jarch3Configuration UNUSED *cfg);
	virtual			~Jarch3Device_Linux_SG();
public:
	virtual bool		open();
	virtual void		close();
public:
	virtual bool		can_provide_residual();
	virtual bool		can_buffer_show_partial_reads();
	virtual bool		can_write_buffer();
	virtual unsigned char*	read_buffer(size_t len);
	virtual size_t		read_buffer_length();
	virtual int		do_scsi(int direction=0,size_t data_length=0);
public:
	int			reserved_size;
	void*			reserved_mmap;
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
	no_mmap = false;
}

/*===================== empty base device ==================*/
Jarch3Device::Jarch3Device(const char *dev,Jarch3Driver *drv,Jarch3Configuration UNUSED *cfg) {
	timeout = -1;
	cmd_length = 0;
	data_length = 0;
	sense_length = 0;
	no_mmap = cfg->no_mmap;
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

int Jarch3Device::do_scsi(int UNUSED direction,size_t UNUSED data_length) {
	errno = ENOSYS;
	return -1;
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

	devobj = new Jarch3Device_Linux_SG(dev,this,cfg);
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
Jarch3Device_Linux_SG::Jarch3Device_Linux_SG(const char *dev,Jarch3Driver *drv,Jarch3Configuration UNUSED *cfg) : Jarch3Device(dev,drv,cfg) {
	reserved_mmap = NULL;
	reserved_size = 0;
}

Jarch3Device_Linux_SG::~Jarch3Device_Linux_SG() {
}

void Jarch3Device_Linux_SG::close() {
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

	return true;
}

bool Jarch3Device_Linux_SG::can_provide_residual() {
	return true;
}

bool Jarch3Device_Linux_SG::can_buffer_show_partial_reads() {
	return true;
}

bool Jarch3Device_Linux_SG::can_write_buffer() {
	return (reserved_mmap != NULL)?true:false; /* yes, IF we are using mmap I/O */
}

unsigned char* Jarch3Device_Linux_SG::read_buffer(size_t len) {
	if (len > (size_t)reserved_size) return NULL;
	/* TODO: Return user-allocated buffer if NOT memory-mapped */
	return (unsigned char*)reserved_mmap;
}

size_t Jarch3Device_Linux_SG::read_buffer_length() {
	return reserved_size;
}

int Jarch3Device_Linux_SG::do_scsi(int direction,size_t data_length) {
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
	sense_length = 0;

	sg.interface_id =			'S';
	sg.cmd_len =				cmd_length;
	sg.cmdp =				cmd;
	sg.mx_sb_len =				sizeof(sense);
	sg.sbp =				sense;
	sg.dxfer_len =				(direction != DirNone) ? data_length : 0;
	sg.dxferp =				(direction != DirNone) ? (reserved_mmap != NULL ? reserved_mmap : NULL/*TODO user buffer*/) : NULL;
	sg.timeout =				(timeout >= 0) ? timeout : 30000;
	/* NTS: OK Linux devs, here's a good example what NOT to do: Don't ever tell me I can set SG_FLAG_MMAP_IO to do the mmap I/O
	 *      but then let me find out the hard way the Linux kernel headers don't actually have such a flag. That serves only to
	 *      piss me the fuck off. --J.C. */
	sg.flags =				(reserved_mmap != NULL ? SG_FLAG_NO_DXFER : SG_FLAG_DIRECT_IO);
	if (direction == DirToHost)		sg.dxfer_direction = SG_DXFER_FROM_DEV;
	else if (direction == DirToDevice)	sg.dxfer_direction = SG_DXFER_TO_DEV;
	else					sg.dxfer_direction = SG_DXFER_NONE;

	r = ioctl(fd,SG_IO,(void*)(&sg));

	sense_length = sg.sb_len_wr;
	if (sg.driver_status != 0)
		fprintf(stderr,"Linux_SG: SG_IO driver_status=0x%lx\n",(unsigned long)sg.driver_status);
	if (sg.masked_status != 0)
		fprintf(stderr,"Linux_SG: SG_IO masked_status=0x%lx\n",(unsigned long)sg.masked_status);
	if (sg.host_status != 0)
		fprintf(stderr,"Linux_SG: SG_IO host_status=0x%lx\n",(unsigned long)sg.host_status);
	if ((sg.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
		fprintf(stderr,"Linux_SG: SG_IO abnormal sense data\n");
		sense_length = 0;
	}

	if ((size_t)sg.resid > (size_t)sg.dxfer_len) {
		fprintf(stderr,"Linux_SG: SG_IO residual > transfer length\n");
		sg.resid = sg.dxfer_len;
	}

	data_length = sg.dxfer_len - sg.resid;
	return r;
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
	fprintf(stderr,"    -no-mmap          Do not use memory-mapped I/O (device driver opt)\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Commands:\n");
	fprintf(stderr,"    eject             Eject CD-ROM tray\n");		/* DONE */
	fprintf(stderr,"    retract           Retract CD-ROM tray\n");		/* DONE */
	fprintf(stderr,"    spinup            Spin up CD-ROM drive\n");		/* DONE */
	fprintf(stderr,"    spindown          Spin down CD-ROM drive\n");	/* DONE */
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
			else if (!strcmp(a,"no-mmap")) {
				cfg.no_mmap = true;
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

/* TODO: Move elsewhere */
static const char *yesno_str[2] = {"No","Yes"};

bool test_unit_ready(Jarch3Device *dev) {
	unsigned char *p;
	int r;

	p = dev->write_command(6);	/* TEST UNIT READY is 6 bytes long */
	if (p == NULL) {
		fprintf(stderr,"%s: cannot obtain command buffer\n",__func__);
		return false;
	}

	memset(p,0,6);		/* set all to zero. TEST UNIT READY command byte is also zero */
	r = dev->do_scsi();
	if (r < 0) {
		fprintf(stderr,"%s: do_scsi() failed, %s\n",__func__,strerror(errno));
		return false;
	}

	return true;
}

bool start_stop_unit(Jarch3Device *dev,unsigned char ctl/*[1:0] = LoEj, Start*/) {
	unsigned char *p;

	p = dev->write_command(6);
	if (p != NULL) {
		p[0] = 0x1B;		/* START STOP UNIT */
		p[4] = ctl;		/* LoEj, Start */
		if (dev->do_scsi() < 0) {
			printf("START STOP UNIT failed\n");
			return false;
		}

		return true;
	}

	return false;
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

	fprintf(stderr,"Opened device %s (driver %s) successfully\n",config.device.c_str(),config.driver.c_str());
	fprintf(stderr,"    can_provide_residual:            %s\n",yesno_str[device->can_provide_residual()?1:0]);
	fprintf(stderr,"    can_buffer_show_partial_reads:   %s\n",yesno_str[device->can_buffer_show_partial_reads()?1:0]);
	fprintf(stderr,"    can_write_buffer:                %s\n",yesno_str[device->can_write_buffer()?1:0]);

	if (config.command == "test-unit-ready") {
		if (test_unit_ready(device)) printf("Test OK\n");
		else printf("Test failed\n");
	}
	else if (config.command == "eject") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		else printf("Test unit ready failed\n");

		if (start_stop_unit(device,0x02)) printf("START STOP UNIT OK\n");
		else printf("START STOP UNIT failed\n");
	}
	else if (config.command == "retract") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		else printf("Test unit ready failed\n");

		if (start_stop_unit(device,0x03)) printf("START STOP UNIT OK\n");
		else printf("START STOP UNIT failed\n");
	}
	else if (config.command == "spinup") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		else printf("Test unit ready failed\n");

		if (start_stop_unit(device,0x01)) printf("START STOP UNIT OK\n");
		else printf("START STOP UNIT failed\n");
	}
	else if (config.command == "spindown") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		else printf("Test unit ready failed\n");

		if (start_stop_unit(device,0x00)) printf("START STOP UNIT OK\n");
		else printf("START STOP UNIT failed\n");
	}

	/* we're finished with the device */
	device->release(); device = NULL;

	/* we're finished with the driver */
	driver->release(); driver = NULL;
	return 0;
}

