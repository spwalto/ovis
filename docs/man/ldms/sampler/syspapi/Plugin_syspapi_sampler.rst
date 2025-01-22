.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for syspapi_sampler ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “30 Sep 2019” “v4” “LDMSD Plugin
syspapi_sampler man page”

.SH NAME Plugin_syspapi_sampler - man page for the LDMSD syspapi_sampler
plugin

.SH SYNOPSIS Within ldmsd_controller or a configuration file: .SY config
.BR name=syspapi_sampler .BI producer= PRODUCER .BI instance= INSTANCE
.OP :raw-latex:`\fBschema`=:raw-latex:`\fISCHEMA` .OP
:raw-latex:`\fBcomponent`\_id=:raw-latex:`\fICOMPONENT`\_ID .OP
:raw-latex:`\fBcfg`\_file=:raw-latex:`\fIPATH` .OP
:raw-latex:`\fBevents`=:raw-latex:`\fIEVENTS` .OP
:raw-latex:`\fBcumulative`=:raw-latex:`\fI0`:raw-latex:`\fR`\|:raw-latex:`\fI1`
.OP
:raw-latex:`\fBauto`\_pause=:raw-latex:`\fI0`:raw-latex:`\fR`\|:raw-latex:`\fI1`
.YS

.SH DESCRIPTION

:raw-latex:`\fBsyspapi`\_sampler:raw-latex:`\fR `collects system-wide
hardware event counters using Linux perf event (see
:raw-latex:`\fBperf`\_event_open:raw-latex:`\fR`(2)), but use PAPI event
names. :raw-latex:`\fBlibpapi`:raw-latex:`\fR `and
:raw-latex:`\fBlibpfm`:raw-latex:`\fR `are used to translate PAPI event
names to Linux perf event attributes. In the case of per-process (job)
data collection, please see
:raw-latex:`\fBPlugin`\_papi_sampler:raw-latex:`\fR`. There are two
approaches to define a list of events: 1)
:raw-latex:`\fBevents`:raw-latex:`\fR `option, or 2) PAPI JSON config
file. For the :raw-latex:`\fBevents`:raw-latex:`\fR `option, simply list
the events of interest separated by comma (e.g.
events=PAPI_TOT_INS,PAPI_TOT_CYC). For the PAPI JSON config file
(:raw-latex:`\fBcfg`\_file:raw-latex:`\fR` option), the format of the
file is as follows: .RS .EX { “schema”: “my_syspapi”, “events”: [ … ] }
.EE .RE The :raw-latex:`\fBschema`:raw-latex:`\fR `is optional, but if
specified in the JSON config file, it precedes the schema name given at
the :raw-latex:`\fBconfig`:raw-latex:`\fR `command. The
:raw-latex:`\fBevents`:raw-latex:`\fR `is a list of PAPI event names
(strings).

If both :raw-latex:`\fBcfg`\_file:raw-latex:`\fR `and
:raw-latex:`\fBevents`:raw-latex:`\fR `options are given to the config
command, the list are concatenated. Please note that an event that
appears on both lists will result in an error.

:raw-latex:`\fBauto`\_pause:raw-latex:`\fR`=:raw-latex:`\fI1`:raw-latex:`\fR `(which
is the default) makes :raw-latex:`\fBsyspapi`\_sampler:raw-latex:`\fR`
paused the data sampling when receiving a notification from
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR` that a job is active, and
resumed the data sampling when receiving a notification from
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `that all jobs have
terminated. This is to prevent perf system resource contention. We have
seen all 0 counters on
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `without any errors (could
be a silent error) when run it with active
:raw-latex:`\fBsyspapi`\_sampler:raw-latex:`\fR`.

.SH CONFIG OPTIONS .TP .BR name=syspapi_sampler This MUST be
syspapi_sampler (the name of the plugin). .TP .BI producer= PRODUCER The
name of the data producer (e.g. hostname). .TP .BI instance= INSTANCE
The name of the set produced by this plugin. .TP .BI schema= SCHEMA The
optional schema name (default: syspapi_sampler). Please note that the
:raw-latex:`\fB`“schema”:raw-latex:`\fR `from the JSON
:raw-latex:`\fBcfg`\_file:raw-latex:`\fR `overrides this option. .TP .BI
component_id= COMPONENT_ID An integer identifying the component
(default: :raw-latex:`\fI0`:raw-latex:`\fR`). .TP .BI cfg_file= PATH The
path to JSON-formatted config file. This is optional if
:raw-latex:`\fBevents`:raw-latex:`\fR `option is specified. Otherwise,
this option is required. .TP .BI events= EVENTS The comma-separated list
of PAPI events of interest (e.g.
:raw-latex:`\fIPAPI`\_TOT_INS,PAPI_TOT_CYC:raw-latex:`\fR`). This is
optional if :raw-latex:`\fBcfg`\_file:raw-latex:`\fR `is specified.
Otherwise, this option is required. .TP .BI cumulative= 0 \| 1
:raw-latex:`\fI0`:raw-latex:`\fR `(default) for non-cumulative data
sampling (reset after read), or :raw-latex:`\fI1`:raw-latex:`\fR `for
cumulative data sampling. .TP .BI auto_pause= 0 \| 1
:raw-latex:`\fI0`:raw-latex:`\fR `to ignore
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `pause/resume notification,
or :raw-latex:`\fI1`:raw-latex:`\fR `(default) to pause/resume according
to notifications from :raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR`.

.SH BUGS No known bugs.

.SH EXAMPLES Plugin configuration example:

.RS .EX load name=syspapi_sampler config name=syspapi_sampler
producer=\ :math:`{HOSTNAME} \\  instance=`\ {HOSTNAME}/syspapi
component_id=2 \\ cfg_file=/tmp/syspapi.json start name=syspapi_sampler
interval=1000000 offset=0 .EE .RE

JSON cfg_file example:

.RS .EX { “events”: [ “PAPI_TOT_INS”, “PAPI_TOT_CYC” ] } .EE .RE

.SH SEE ALSO .nh .BR Plugin_papi_sampler (7), .BR ldmsd (8), .BR
ldms_quickstart (7), .BR ldmsd_controller (8), .BR ldms_sampler_base
(7).
