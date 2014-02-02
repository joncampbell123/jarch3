
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

/* Fuck you Linux headers. Put the useful #defines we actually need here */
#ifndef SG_FLAG_DIRECT_IO
#define SG_FLAG_DIRECT_IO 1     /* default is indirect IO */
#endif

#ifndef SG_FLAG_UNUSED_LUN_INHIBIT
#define SG_FLAG_UNUSED_LUN_INHIBIT 2   /* default is overwrite lun in SCSI */
#endif

#ifndef SG_FLAG_MMAP_IO
#define SG_FLAG_MMAP_IO 4       /* request memory mapped IO */
#endif

#ifndef SG_FLAG_NO_DXFER
#define SG_FLAG_NO_DXFER 0x10000 /* no transfer of kernel buffers to/from */
				/* user space (debug indirect IO) */
#endif

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
	unsigned long		sector;
	unsigned char		page;
	unsigned char		subpage;
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
	virtual int		do_scsi(int direction=0,size_t n_data_length=0);
	virtual void		dump_sense(FILE *fp=NULL);
public:
	int			fd;
	string			device;
	Jarch3Driver*		driver;

	unsigned char		sense[128];
	size_t			sense_length;

	unsigned char		cmd[32];
	size_t			cmd_length;

	int			timeout;
	size_t			data_length;

	bool			auto_fetch_sense;			/* default true: automatically fetch sense if system fails to provide it */
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
	virtual int		do_scsi(int direction=0,size_t n_data_length=0);
public:
	int			reserved_size;
	void*			reserved_mmap;
	void*			user_buffer;
};

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

static const char *scsi_keys[16] = {
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

static const char *scsi_asc(unsigned char key,unsigned char asc,unsigned char ascq) {
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
			scsi_keys[sense[2]&0xF],
			scsi_asc(sense[2]&0xF,sense[12],sense[13]));
	}
}

int Jarch3Device::do_scsi(int UNUSED direction,size_t UNUSED n_data_length) {
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

/* TODO: Move elsewhere */
static const char *yesno_str[2] = {"No","Yes"};

#define TUR_EXPECT_MEDIUM_NOT_PRESENT		0x01

bool test_unit_ready(Jarch3Device *dev,unsigned int expect=0) {
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
					if (expect&TUR_EXPECT_MEDIUM_NOT_PRESENT)
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

bool start_stop_unit(Jarch3Device *dev,unsigned char ctl/*[1:0] = LoEj, Start*/) {
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

bool prevent_allow_medium_removal(Jarch3Device *dev,unsigned char ctl) {
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

bool seek_cdrom(Jarch3Device *dev,unsigned long sector) {
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

bool pause_resume_audio(Jarch3Device *dev,unsigned char resume) {
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

bool play_audio(Jarch3Device *dev,unsigned long sector) {
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

int read10(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects) {
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

int read12(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects) {
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

int readcd(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects,unsigned char expected_sector_type,unsigned char dap,unsigned char b9,unsigned char b10) {
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

int get_capacity(void *dst,size_t dstmax,Jarch3Device *dev) {
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

int get_configuration(void *dst,size_t dstmax,Jarch3Device *dev) {
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

int get_configuration_profile_only(void *dst,size_t dstmax,Jarch3Device *dev) {
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

int read_subchannel_curpos(void *dst,size_t dstmax,Jarch3Device *dev,unsigned char MSF,unsigned char SUBQ) {
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

int mode_sense(void *dst,size_t dstmax,Jarch3Device *dev,unsigned char PAGE,unsigned char SUBPAGE) {
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

const char *mmc_profile_to_str(unsigned int p) {
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
		if (test_unit_ready(device,TUR_EXPECT_MEDIUM_NOT_PRESENT)) printf("Test unit ready OK\n");
		if (start_stop_unit(device,0x02)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "retract") {
		if (test_unit_ready(device,TUR_EXPECT_MEDIUM_NOT_PRESENT)) printf("Test unit ready OK\n");
		if (start_stop_unit(device,0x03)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "spinup") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (start_stop_unit(device,0x01)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "spindown") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (start_stop_unit(device,0x00)) printf("START STOP UNIT OK\n");
	}
	else if (config.command == "lock") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (prevent_allow_medium_removal(device,0x01)) printf("PREVENT ALLOW MEDIUM REMOVAL OK\n");
	}
	else if (config.command == "unlock") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (prevent_allow_medium_removal(device,0x00)) printf("PREVENT ALLOW MEDIUM REMOVAL OK\n");
	}
	else if (config.command == "seek") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (seek_cdrom(device,config.sector)) printf("SEEK OK\n");
	}
	else if (config.command == "play-audio") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (play_audio(device,config.sector)) printf("PLAY AUDIO OK\n");
	}
	else if (config.command == "pause-audio") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (pause_resume_audio(device,/*resume=*/0)) printf("PAUSE OK\n");
	}
	else if (config.command == "resume-audio") {
		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if (pause_resume_audio(device,/*resume=*/1)) printf("RESUME OK\n");
	}
	else if (config.command == "mode-sense") {
		unsigned char buffer[256];
		int rd,i;

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=mode_sense(buffer,sizeof(buffer),device,config.page,config.subpage)) < 0) printf("RESUME OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");
	}
	else if (config.command == "read-subchannel") {
		unsigned char buffer[256];
		int rd,i;

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=read_subchannel_curpos(buffer,sizeof(buffer),device,/*MSF=*/1,/*SUBQ=*/1)) < 0) printf(" OK\n");

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=get_configuration_profile_only(buffer,sizeof(buffer),device)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		/* NTS: Data Length field @0 is 32-bit wide and contains the length of data following the field.
		 *      Typically that means a total response of 424 bytes has a Data Length field of 420 */
		printf("Feature header: datalen=%u (when returned %u) current_profile=0x%04x %s\n",
			((unsigned int)buffer[0] << 24) + ((unsigned int)buffer[1] << 16) +
			((unsigned int)buffer[2] <<  8) + ((unsigned int)buffer[3]),
			rd,((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7]),
			mmc_profile_to_str(((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7])));
	}
	else if (config.command == "get-config") {
		unsigned char buffer[16384],*s,*f;
		int rd,i;

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=get_configuration(buffer,sizeof(buffer),device)) < 0) printf(" OK\n");

		for (i=0;i < rd;i++) printf("0x%02x ",buffer[i]);
		printf("\n");

		/* NTS: Data Length field @0 is 32-bit wide and contains the length of data following the field.
		 *      Typically that means a total response of 424 bytes has a Data Length field of 420 */
		printf("Feature header: datalen=%u (when returned %u) current_profile=0x%04x %s\n",
			((unsigned int)buffer[0] << 24) + ((unsigned int)buffer[1] << 16) +
			((unsigned int)buffer[2] <<  8) + ((unsigned int)buffer[3]),
			rd,((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7]),
			mmc_profile_to_str(((unsigned int)buffer[6] << 8) + ((unsigned int)buffer[7])));

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=get_capacity(buffer,sizeof(buffer),device)) < 0) printf(" OK\n");

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=read10(buffer,sizeof(buffer),device,config.sector,1)) < 0) printf(" OK\n");

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=read12(buffer,sizeof(buffer),device,config.sector,1)) < 0) printf(" OK\n");

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=readcd(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=MODE-1*/2,/*dap=*/0,/*b9=*/0x10,/*b10=*/0x00)) < 0) printf(" OK\n");

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=readcd(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=MODE-2 FORM-1*/4,/*dap=*/0,/*b9=*/0x10,/*b10=*/0x00)) < 0) printf(" OK\n");

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

		if (test_unit_ready(device)) printf("Test unit ready OK\n");
		if ((rd=readcd(buffer,sizeof(buffer),device,config.sector,1,/*sector_type=any*/0,/*dap=*/0,/*b9=*/0xF8,/*b10=*/0x00)) < 0) printf(" OK\n");
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

