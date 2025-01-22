." Manpage for Plugin_store_timescale ." Contact ovis-help@ca.sandia.gov
to correct errors or typos. .TH man 7 “24 Oct 2019” “v4” “LDMS Plugin
store_timescale man page”

.SH NAME Plugin_store_timescale - man page for the LDMS store_timescale
plugin

.SH SYNOPSIS Within ldmsd_controller script or a configuration file: .br
load name=store_timescale .br strgp_add name= plugin=store_timescale
container= schema= .br strgp_prdcr_add name= regex=.\* .br strgp_start
name= .br

.SH DESCRIPTION With LDMS (Lightweight Distributed Metric Service),
plugins for the ldmsd (ldms daemon) are configured via ldmsd_controller
or a configuration file. The timescale_store plugin is a store developed
by Shanghai Jiao Tong University HPC Center to store collected data in
TimescaleDB.

This store is a simplified version of store_influx. .PP

.SH STORE_TIMESCALE CONFIGURATION ATTRIBUTE SYNTAX .TP .BR config name=
user= pwfile= hostaddr= port= dbname= measurement_limit= .br
ldmsd_controller configuration line .RS .TP name= .br This MUST be
store_timescale. .TP user= .br This option is required; It will be used
as the user name to connect to timescaledb. .TP pwfile= .br This option
is required; The file must have content secretword=, the password will
be used as the password to connect to timescaledb. .TP hostaddr= .br
This option is required; It will be used as the ip addr of timescaledb
to connect to. .TP port= .br This option is required; It will be used as
the port number of timescaledb to connect to. .TP dbname= .br This
option is required; It will be used as the timescaledb database name to
connect to. .TP measurement_limit= .br This is optional; It specifies
the maximum length of the sql statement to create table or insert data
into timescaledb; default 8192. .RE

.SH STRGP_ADD ATTRIBUTE SYNTAX The strgp_add sets the policies being
added. This line determines the output files via identification of the
container and schema. .TP .BR strgp_add plugin=store_timescale name=
schema= container= .br ldmsd_controller strgp_add line .br .RS .TP
plugin= .br This MUST be store_timescale. .TP name= .br The policy name
for this strgp. .TP container= .br The container and the schema
determine where the output files will be written (see path above). .TP
schema= .br The container and the schema determine where the output
files will be written (see path above). You can have multiples of the
same sampler, but with different schema (which means they will have
different metrics) and they will be stored in different containers (and
therefore files). .RE

.SH STORE COLUMN ORDERING

This store generates output columns in a sequence influenced by the
sampler data registration. Specifically, the column ordering is .PP .RS
Time, Time_usec, ProducerName, \* .RE .PP The column sequence of is the
order in which the metrics are added into the metric set by the sampler.
.PP

.SH NOTES None.

.SH BUGS None known.

.SH EXAMPLES .PP Within ldmsd_controller or in a ldmsd command script
file .nf load name=store_timescale .br strgp_add name=store_tutorial1
plugin=store_timescale schema=test1 container=tutorial_sampler1 .br
strgp_prdcr_add name=store_tutorial1 regex=.\ *.br strgp_start
name=store_tutorial1 .br strgp_add name=store_tutorial2
plugin=store_tutorial schema=test2 container=tutorial_sampler2 .br
strgp_prdcr_add name=store_tutorial2 regex=.* .br strgp_start
name=store_tutorial2 .br strgp_add name=store_tutorial3
plugin=store_tutorial schema=test3 container=tutorial_sampler3 .br
strgp_prdcr_add name=store_tutorial3 regex=.\* .br strgp_start
name=store_tutorial3 .br .fi

.SH SEE ALSO ldmsd(8), ldms_quickstart(7), ldmsd_controller(8),
Plugin_tutorial_sampler(7), Plugin_store_csv(7)
