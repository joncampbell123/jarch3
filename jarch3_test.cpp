
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include <string>

using namespace std;

class Jarch3Configuration {
public:
	Jarch3Configuration() {
		reset();
	}
	~Jarch3Configuration() {
	}
public:
	void reset() {
		driver = "linux_sg";
		device = "/dev/dvd";
		command.clear();
	}
public:
	string			driver;
	string			device;
	string			command;
};

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

	if (parse_argv(config,argc,argv))
		return 1;

	return 0;
}

