
#ifndef _JARCH3_JARCH3_H
#define _JARCH3_JARCH3_H

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

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

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

#define JARCH3_TUR_EXPECT_MEDIUM_NOT_PRESENT		0x01

extern const char *jarch3_yesno_str[2];
extern const char *jarch3_scsi_keys[16];

const char *jarch3_mmc_profile_to_str(unsigned int p);
const char *jarch3_scsi_asc(unsigned char key,unsigned char asc,unsigned char ascq);

class Jarch3Configuration {
public:
				Jarch3Configuration();
				~Jarch3Configuration();
	void			reset();
public:
	std::string		driver;
	std::string		device;
	std::string		command;
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
	std::string		device;
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

bool jarch3_test_unit_ready(Jarch3Device *dev,unsigned int expect=0);
bool jarch3_start_stop_unit(Jarch3Device *dev,unsigned char ctl/*[1:0] = LoEj, Start*/);
bool jarch3_prevent_allow_medium_removal(Jarch3Device *dev,unsigned char ctl);
bool jarch3_seek_cdrom(Jarch3Device *dev,unsigned long sector);
bool jarch3_pause_resume_audio(Jarch3Device *dev,unsigned char resume);
bool jarch3_play_audio(Jarch3Device *dev,unsigned long sector);
int jarch3_read10(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects);
int jarch3_read12(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects);
int jarch3_readcd(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects,unsigned char expected_sector_type,unsigned char dap,unsigned char b9,unsigned char b10);
int jarch3_readmsf(void *dst,size_t dstmax,Jarch3Device *dev,unsigned long lba,unsigned int sects,unsigned char expected_sector_type,unsigned char dap,unsigned char b9,unsigned char b10);
int jarch3_get_capacity(void *dst,size_t dstmax,Jarch3Device *dev);
int jarch3_get_configuration(void *dst,size_t dstmax,Jarch3Device *dev);
int jarch3_get_configuration_profile_only(void *dst,size_t dstmax,Jarch3Device *dev);
int jarch3_read_subchannel_curpos(void *dst,size_t dstmax,Jarch3Device *dev,unsigned char MSF,unsigned char SUBQ);
int jarch3_mode_sense(void *dst,size_t dstmax,Jarch3Device *dev,unsigned char PAGE,unsigned char SUBPAGE);

Jarch3Driver *Jarch3GetDriver(std::string &driver,std::string UNUSED &device,Jarch3Configuration UNUSED *cfg);

#endif /* _JARCH3_JARCH3_H */

