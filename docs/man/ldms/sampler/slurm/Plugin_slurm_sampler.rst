.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for slurm_sampler ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “30 Sep 2019” “v4” “LDMSD Plugin
slurm_sampler man page”

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/
.SH NAME Plugin_slurm_sampler - man page for the LDMSD slurm_sampler
plugin

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/
.SH SYNOPSIS

Within ldmsd_controller or a configuration file: .SY config .BR
name=slurm_sampler .BI producer= PRODUCER .BI instance= INSTANCE .OP
:raw-latex:`\fBcomponent`\_id=:raw-latex:`\fICOMP`\_ID .OP
:raw-latex:`\fBstream`=:raw-latex:`\fISTREAM` .OP
:raw-latex:`\fBjob`\_count=:raw-latex:`\fIMAX`\_JOBS .OP
:raw-latex:`\fBtask`\_count=:raw-latex:`\fIMAX`\_TASKS .YS

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/
.SH DESCRIPTION

:raw-latex:`\fBslurm`\_sampler:raw-latex:`\fR `is a sampler plugin that
collects the information of the Slurm jobs running on the node. It
subscribes to the specified :raw-latex:`\fBstream`:raw-latex:`\fR `to
which the :raw-latex:`\fBslurm`\_notifier:raw-latex:`\fR `SPANK plugin
(see :raw-latex:`\fBPlugin`\_slurm_notifier:raw-latex:`\fR`(7)) publish
Slurm job events (default stream:
:raw-latex:`\fIslurm`:raw-latex:`\fR`). The sampler supports
multi-tenant jobs.

The :raw-latex:`\fBjob`\_count:raw-latex:`\fR `option is the number of
slots in the LDMS set allocated for concurrent jobs. If the number of
concurrent jobs on the node is greater than
:raw-latex:`\fBjob`\_count:raw-latex:`\fR`, the new job will occupy the
slot of the oldest job. If :raw-latex:`\fBjob`\_count:raw-latex:`\fR `is
not specified, the default value is :raw-latex:`\fI8`:raw-latex:`\fR`.

The :raw-latex:`\fBtask`\_count:raw-latex:`\fR `is the maximum number of
tasks per job on the node. If not specified, it is
:raw-latex:`\fICPU`\_COUNT:raw-latex:`\fR`. In the event of the sampler
failed to obtain :raw-latex:`\fICPU`\_COUNT:raw-latex:`\fR`, the default
value is :raw-latex:`\fI64`:raw-latex:`\fR`.

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/
.SH CONFIG OPTIONS

.TP .BR name=slurm_sampler This MUST be slurm_sampler (the name of the
plugin). .TP .BI producer= PRODUCER The name of the data producer
(e.g. hostname). .TP .BI instance= INSTANCE The name of the set produced
by this plugin. This option is required. .TP .BI component_id=
COMPONENT_ID An integer identifying the component (default:
:raw-latex:`\fI0`:raw-latex:`\fR`). .TP .BI stream= STREAM The name of
the LDMSD stream to get the job event data. .TP .BI job_count= MAX_JOBS
The number of slots to hold job information. If all slots are occupied
at the time the new job arrived, the oldest slot is reused. The default
value is :raw-latex:`\fI8`:raw-latex:`\fR`. .TP .BI task_count=
MAX_TASKS The number of slots for tasks information per job. If not
specified, the sampler will try to obtain system CPU_COUNT and use it as
task_count. If it failed, the default value is
:raw-latex:`\fI64`:raw-latex:`\fR`.

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/
.SH BUGS

No known bugs.

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/
.SH EXAMPLES

Plugin configuration example:

.RS .EX load name=slurm_sampler config name=slurm_sampler
producer=\ :math:`{HOSTNAME} instance=`\ {HOSTNAME}/slurm \\
component_id=2 stream=slurm job_count=8 task_count=8 start
name=slurm_sampler interval=1000000 offset=0 .EE .RE

.SH SEE ALSO .nh .BR ldmsd (8), .BR ldms_quickstart (7), .BR
ldmsd_controller (8), .BR ldms_sampler_base (7).
