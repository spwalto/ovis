.. role:: raw-latex(raw)
   :format: latex
..

." Man page for slurm_notifier ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “30 Sep 2019” “v4” “SPANK Plugin
slurm_notifier man page”

.SH NAME Plugin_slurm_notifier - man page for the SPANK slurm_notifier
plugin

.SH SYNOPSIS Within plugstack.conf: .SY required .IR OVIS_PREFIX /
LIBDIR /ovis-ldms/libslurm_notifier.so .BI stream= STREAM_NAME .BI
timeout= TIMEOUT_SEC .BI [user_debug] .BI client= XPRT : HOST : PORT :
AUTH .RB … .YS

.SH DESCRIPTION

:raw-latex:`\fBslurm`\_notifier:raw-latex:`\fR `is a SPANK plugin that
notifies :raw-latex:`\fBldmsd`:raw-latex:`\fR `about job events
(e.g. job start, job termination) and related information (e.g. job_id,
task_id, task process ID). The notification is done over
:raw-latex:`\fBldmsd`\_stream:raw-latex:`\fR` publish mechanism. See
SUBSCRIBERS below for plugins known to consume the spank plugin
messages.

.BI stream= STREAM_NAME specifies the name of publishing stream. The
default value is :raw-latex:`\fIslurm`:raw-latex:`\fR`.

.BI timeout= TIMEOUT_SEC is the number of seconds determining the
time-out of the LDMS connections (default
:raw-latex:`\fI5`:raw-latex:`\fR`).

.BI user_debug, if present, enables sending certain plugin management
debugging messages to the user’s slurm output. (default: disabled –
slurm_debug2() receives the messages instead).

.BI client= XPRT : HOST : PORT : AUTH specifies
:raw-latex:`\fBldmsd`:raw-latex:`\fR `to which
:raw-latex:`\fBslurm`\_notifier:raw-latex:`\fR `publishes the data. The
:raw-latex:`\fIXPRT`:raw-latex:`\fR `specifies the type of the
transport, which includes .BR sock “,” rdma “,” ugni “, and” fabric .
The :raw-latex:`\fIHOST`:raw-latex:`\fR `is the hostname or the IP
address that :raw-latex:`\fBldmsd`:raw-latex:`\fR `resides. The
:raw-latex:`\fIPORT`:raw-latex:`\fR `is the listening port of the
:raw-latex:`\fBldmsd`:raw-latex:`\fR`. The
:raw-latex:`\fIAUTH`:raw-latex:`\fR `is the LDMS authentication method
that the :raw-latex:`\fBldmsd`:raw-latex:`\fR `uses, which are .BR munge
“, or” none . The :raw-latex:`\fBclient`:raw-latex:`\fR `option can be
repeated to specify multiple :raw-latex:`\fBldmsd`:raw-latex:`\fR`’s.

.SH SUBSCRIBERS The following plugins are known to process
slurm_notifier messages: .nf slurm_sampler (collects slurm job & task
data) slurm_sampler2 (collects slurm job & task data) papi_sampler
(collects PAPI data from tasks identified) linux_proc_sampler (collects
/proc data from tasks identified) .fi

.SH EXAMPLES /etc/slurm/plugstack.conf:

| .RS .EX required /opt/ovis/lib64/ovis-ldms/libslurm_notifier.so
| stream=slurm timeout=5 client=sock:localhost:10000:munge
| client=sock:node0:10000:munge .EE .RE

.SH SEE ALSO .nh .BR spank (8), .BR Plugin_slurm_sampler (7), .BR
Plugin_papi_sampler (7), .BR Plugin_linux_proc_sampler (7), .BR ldmsd
(8), .BR ldms_quickstart (7),
