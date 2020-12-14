/*
 * tx2mon.h -	LDMS sampler for basic Marvell TX2 chip telemetry.
 */

/*
 *  Copyright [2020] Hewlett Packard Enterprise Development LP
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License as published 
 * by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with 
 * this program; if not, write to:
 * 
 *   Free Software Foundation, Inc.
 *   51 Franklin Street, Fifth Floor
 *   Boston, MA 02110-1301, USA.
 */

/*
 * Header file from github.com/jchandra-cavm/tx2mon.git, which
 * is required to be checked out elsewhere on the build system,
 * and pointed to during the configure phase.
 *
 * TODO: insert exact configure command line required here.
 */
#include <tx2mon/mc_oper_region.h>

#include <limits.h>
/*
 * Forward declarations.
 */
static void term(struct ldmsd_plugin *self);
static int config(struct ldmsd_plugin *self, struct attr_value_list *kwl, struct attr_value_list *avl);
static const char *usage(struct ldmsd_plugin *self);
static ldms_set_t get_set(struct ldmsd_sampler *self);			/* Obsolete */
static int sample(struct ldmsd_sampler *self);
static int create_metric_set(base_data_t base);

//Definitions and functions used to parse and dump cpu data to screen
#define CORES_PER_ROW 4
#define PIDFMAX 32
#define BUFMAX 512
//#define debug
/*
 * Location of the sysfs entries created by the kernel module.
 *
 * NOTE: TX2MON_NODE_PATH is a snprintf() string to create actual path.
 */
#define	TX2MON_SYSFS_PATH	"/sys/bus/platform/devices/tx2mon/"
#define	TX2MON_SOCINFO_PATH	TX2MON_SYSFS_PATH "socinfo"
#define TX2MON_NODE_PATH      TX2MON_SYSFS_PATH "node%d_raw"
/*
 * Max number of CPUs (TX2 chips) supported.
 */
#define	TX2MON_MAX_CPU	(2)

/*
 * Size of temporary buffer used when registering schema etc
 */
#define	TMP_BUF_SIZE	(512)

/*
 * Set of possible capabilities supported by the kernel module.
 */
typedef enum {CAP_BASIC = 0x00, CAP_THROTTLE = 0x01} tx2mon_cap;
	
/*
 * Per-CPU record keeping
 */
struct cpu_info {
	int	fd;		/* fd of raw file opened */
	int	metric_offset;	/* starting offset into schema for this CPU */
	struct	mc_oper_region mcp;	/* mmapped data structure (from fd) */
	unsigned int throttling_available:1;
	int	node;
};

/*
 * Per-sampler record keeping
 */
struct tx2mon_sampler {
	int	n_cpu;		/* number of CPUs (e.g. TX2 chips) present */
	int	n_core;		/* cores *per CPU* */
	int	n_thread;	/* threads *per core* (unused currently) */
	tx2mon_cap	cap;	/* capabilites of kernel module */
	
	FILE	*fileout;
	int 	samples;
	struct cpu_info cpu[TX2MON_MAX_CPU];
} ;

/*brief parse on soc info and fill provides struct.
   * \return 0 on success, errno from fopen, ENODATA from
    * failed fgets, ENOKEY or ENAMETOOLONG from failed parse.
    */
static int parse_socinfo(void);

/* Read the information located in th the node file directory*/
static int read_cpu_info(struct cpu_info *s);

/*Read and query cpu data for each node and map to data strucutre. Can also be used for debugging
 * by displaying the data to the ldmsd log file*/
static int parse_mc_oper_region();

/* Define metric list and call tx2mon_array_conv */
static int tx2mon_set_metrics(int i);

/* Convert metric values to temp, voltage and power units. Output results in float and uint32_t types */
static int tx2mon_array_conv(void *s, int p, int idx, int i, uint32_t t);

#ifdef debug
static char *get_throttling_cause(unsigned int active_event, const char *sep, char *buf, int bufsz);
static struct termios *ts_saved;
static int display_extra = 1;
static int display_throttling = 1;

static struct term_seq {
        char *cl;
        char *nl;
} term_seq;

#endif
