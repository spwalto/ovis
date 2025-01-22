.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for papi_sampler ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “30 Sep 2019” “v4” “LDMSD Plugin
papi_sampler man page”

.SH NAME Plugin_papi_sampler - man page for the LDMSD papi_sampler
plugin

.SH SYNOPSIS Within ldmsd_controller or a configuration file: .SY config
.BR name=papi_sampler .BI producer= PRODUCER .BI instance= INSTANCE .OP
:raw-latex:`\fBcomponent`\_id=:raw-latex:`\fICOMP`\_ID .OP
:raw-latex:`\fBstream`=:raw-latex:`\fISTREAM` .OP
:raw-latex:`\fBjob`\_expiry=:raw-latex:`\fIEXPIRY`\_SEC .YS

.SH DESCRIPTION :raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `monitors
PAPI events of processes of Slurm jobs.

The job script must define
:raw-latex:`\fBSUBSCRIBER`\_DATA:raw-latex:`\fR `environment variable as
a JSON object that has at least
:raw-latex:`\fB`“papi_sampler”:raw-latex:`\fR `attribute as follows:

.RS .EX SUBSCRIBER_DATA=‘{“papi_sampler”:{“file”:“/PATH/TO/PAPI.JSON”}}’
.EE .RE

where the :raw-latex:`\fB`“file”:raw-latex:`\fR `attribute inside
:raw-latex:`\fB`“papi_sampler”:raw-latex:`\fR `points to a
JSON-formatted text file containing user-defined schema name and PAPI
events of interest, e.g.

.RS .EX { “schema”: “my_papi”, “events”: [ “PAPI_TOT_INS”, “PAPI_L1_DCM”
] } .EE .RE

:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `relies on
:raw-latex:`\fBslurm`\_notfifier:raw-latex:`\fR `SPANK plugin to notify
it about the starting/stopping of jobs on the node over ldmsd_stream.
Please consult
:raw-latex:`\fBPlugin`\_slurm_notifier(7):raw-latex:`\fR `for more
information on how to deploy and configure it. The value of
SUBSCRIBER_DATA from the job script is carried over to
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `when the job started, and
an LDMS set will be created according to the PAPI JSON file pointed by
the SUBSCRIBER_DATA. In the case of multi-tenant (multiple jobs running
on a node), each job has its own set. The set is deleted after
:raw-latex:`\fIjob`\_expiry:raw-latex:`\fR `period after the job exited.

.SH CONFIG OPTIONS .TP .BR name=papi_sampler This MUST be papi_sampler
(the name of the plugin). .TP .BI producer= PRODUCER The name of the
data producer (e.g. hostname). .TP .BI instance= INSTANCE This is
mandatory due to the fact that
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `extends
:raw-latex:`\fBsampler`\_base:raw-latex:`\fR `and this option is
required by :raw-latex:`\fBsampler`\_base:raw-latex:`\fR `config.
However, the value is ignored and can be anything. The actual name of
the :raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `instance is .IR
PRODUCER / SCHEMA / JOB_ID . .TP .BI component_id= COMPONENT_ID An
integer identifying the component (default:
:raw-latex:`\fI0`:raw-latex:`\fR`). .TP .BI stream= STREAM The name of
the stream that :raw-latex:`\fBslurm`\_notifier:raw-latex:`\fR `SPANK
plugin uses to notify the job events. This attribute is optional with
the default being :raw-latex:`\fIslurm`:raw-latex:`\fR`. .TP .BI
job_expiry= EXPIRY_SEC The number of seconds to retain the set after the
job has exited. The default value is :raw-latex:`\fI60`:raw-latex:`\fR`.

.SH BUGS No known bugs.

.SH EXAMPLES Plugin configuration example:

.RS .EX load name=papi_sampler config name=papi_sampler producer=node0
instance=NA component_id=2 job_expiry=10 start name=papi_sampler
interval=1000000 offset=0 .EE .RE

Job script example:

.RS .EX #!/bin/bash export
SUBSCRIBER_DATA=‘{“papi_sampler”:{“file”:“/tmp/papi.json”}}’ srun bash
-c ‘for X in {1..60}; do echo $X; sleep 1; done’ .EE .RE

PAPI JSON example:

.RS .EX { “schema”: “my_papi”, “events”: [ “PAPI_TOT_INS”, “PAPI_L1_DCM”
] } .EE .RE

.SH SEE ALSO .nh .BR Plugin_slurm_notifier (7), .BR
Plugin_syspapi_sampler (7), .BR ldmsd (8), .BR ldms_quickstart (7), .BR
ldmsd_controller (8), .BR ldms_sampler_base (7).
