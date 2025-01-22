." Manpage for Plugin_papi ." Contact ovis-help@ca.sandia.gov to correct
errors or typos. .TH man 7 “09 May 2016” “v3” “LDMS Plugin papi man
page”

.SH NAME Plugin_papi - man page for the LDMS papi sampler plugin.

.SH SYNOPSIS

Within ldmsctl .br ldmsctl> config name=spapi [ = ]

.SH DESCRIPTION With LDMS (Lightweight Distributed Metric Service),
plugins for the ldmsd (ldms daemon) are configured via ldmsctl. The papi
sampler plugin runs on the nodes and provides data about the the
occurrence of micro-architectural events using papi library by accessing
hardware performance counters.

.SH ENVIRONMENT

You will need to build LDMS with –enable-papi. Papi library should be
available through plugin library path.

.SH LDMSCTL CONFIGURATION ATTRIBUTE SYNTAX

.TP .BR config

name= events= pid= producer= instance= [schema=] [component_id=
with_jobid=] ldmsctl configuration line .TP name= .br This MUST be
spapi. .TP producer= .br The producer string value. .TP instance= .br
The name of the metric set .TP schema= .br Optional schema name. It is
intended that the same sampler on different nodes with different metrics
have a different schema. .TP component_id= .br Optional component
identifier. Defaults to zero. .TP with_jobid= .br Option to collect job
id with set or 0 if not. .TP events= .br Comma separated list of events.
Available events can be determined using papi_avail command if papi is
installed on system. .TP pid - The PID for the process being monitored
.br

.RE

.SH NOTES .PP In order to check if an event is available on the system
you can run papi_avail.

.SH BUGS No known bugs.

.SH EXAMPLES .PP .TP The following is a short example that measures 4
events. .br Total CPU cycles .br Total CPU instructions .br Total branch
instructions .br Mispredicted branch instructions

.PP

$ldmsctl -S $LDMSD_SOCKPATH

ldmsctl> load name=spapi .br ldmsctl> config name=spapi
producer=\ :math:`PRODUCER_NAME instance=`\ INSTANCE_NAME
pid=\ :math:`PID events=PAPI_TOT_INS,PAPI_TOT_CYC,PAPI_BR_INS,PAPI_BR_MSP .br ldmsctl> start name=spapi interval=`\ INTERVAL_VALUE
.br ldmsctl> quit

.SH SEE ALSO

papi_avail(1) , ldmsd(7), ldms_quickstart(7)
