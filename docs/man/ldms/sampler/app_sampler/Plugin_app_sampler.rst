.. role:: raw-latex(raw)
   :format: latex
..

.TH man 7 “30 Sep 2019” “v4” “LDMSD Plugin app_sampler man page”

.ad l .nh

.SH “NAME” .PP .PP ldmsd_app_sampler - LDMSD app_sampler plugin .PP .SH
“SYNOPSIS” .SY config .BR name=app_sampler .BI producer= PRODUCER .BI
instance= INSTANCE .OP :raw-latex:`\fBschema`=:raw-latex:`\fISCHEMA` .OP
:raw-latex:`\fBcomponent`\_id=:raw-latex:`\fICOMPONENT`\_ID .OP
:raw-latex:`\fBstream`=:raw-latex:`\fISTREAM`\_NAME .OP
:raw-latex:`\fBmetrics`=:raw-latex:`\fIMETRICS` .OP
:raw-latex:`\fBcfg`\_file=:raw-latex:`\fIPATH` .YS .PP .PP .SH
“DESCRIPTION” .PP .PP
:raw-latex:`\f[`CB]app_sampler:raw-latex:`\fR `collects metrics from
:raw-latex:`\f[`CB]/proc/:raw-latex:`\fR `according to current SLURM
jobs/tasks running on the system&.
:raw-latex:`\f[`CB]app_sampler:raw-latex:`\fR `depends on
:raw-latex:`\f[`CB]slurm_notifier:raw-latex:`\fR `SPANK plugin to send
SLURM job/task events over
:raw-latex:`\f[`CB]ldmsd_stream:raw-latex:`\fR `(:raw-latex:`\f[`CB]stream:raw-latex:`\fR `option,
default: slurm)&. A set is created per task when the task started in the
following format:
:raw-latex:`\f[`CB]PRODUCER_NAME/JOB_ID/TASK_PID:raw-latex:`\fR`&. The
set is deleted when the task exited&. .PP By default
:raw-latex:`\f[`CB]app_sampler:raw-latex:`\fR `sampling all available
metrics (see :raw-latex:`\f[`CB]LIST OF
METRICS:raw-latex:`\fR `section)&. Users may down-select the list of
metrics to monitor by specifying
:raw-latex:`\f[`CB]metrics:raw-latex:`\fR `option (comma-separated
string) or writing a JSON configuration file and specifying
:raw-latex:`\f[`CB]cfg_file:raw-latex:`\fR `option (see
:raw-latex:`\f[`CB]EXAMPLES:raw-latex:`\fR `section)&. .PP .SH “CONFIG
OPTIONS” .PP .PP .IP “:raw-latex:`\fBname `:raw-latex:`\fR`” 1c Must be
app_sampler. .IP “:raw-latex:`\fBproducer `:raw-latex:`\fR`” 1c The name
of the data producer (e.g. hostname). .IP
“:raw-latex:`\fBinstance `:raw-latex:`\fR`” 1c This is required by
sampler_base but is not used by app_sampler. So, this can be any string
but must be present. .IP “:raw-latex:`\fBschema `:raw-latex:`\fR`” 1c
The optional schema name (default: app_sampler). .IP
":raw-latex:`\fBcomponent`\_id :raw-latex:`\fR`" 1c An integer
identifying the component (default: :raw-latex:`\fI0`:raw-latex:`\fR`).
.IP “:raw-latex:`\fBstream `:raw-latex:`\fR`” 1c The name of the
:raw-latex:`\f[`CB]ldmsd_stream:raw-latex:`\fR `to listen for SLURM job
events&. (default: slurm)&. .IP
“:raw-latex:`\fBmetrics `:raw-latex:`\fR`” 1c The comma-separated list
of metrics to monitor&. The default is ’’ (empty), which is equivalent
to monitor ALL metrics&. .IP ":raw-latex:`\fBcfg`\_file
:raw-latex:`\fR`" 1c The alternative config file in JSON format&. The
file is expected to have an object that may contain the following
attributes: .PP .RS 4 .nf

::

       {
               'stream': 'STREAM_NAME'
               'metrics': [ METRICS ]
       }

.fi .RE .PP The default values are assumed for the attributes that are
not specified&. Attributes other than ‘stream’ and ‘metrics’ are
ignored&. .PP If the :raw-latex:`\f[`CB]cfg_file:raw-latex:`\fR `is
given, :raw-latex:`\f[`CB]stream:raw-latex:`\fR `and
:raw-latex:`\f[`CB]metrics:raw-latex:`\fR `options are ignored&. .PP .PP
.SH “LIST OF METRICS” .PP .PP .PP .RS 4 .nf /\* from /proc/[pid]/cmdline
\*/ cmdline_len, cmdline,

/\* the number of open files \*/ n_open_files,

/\* from /proc/[pid]/io \*/ io_read_b, io_write_b, io_n_read,
io_n_write, io_read_dev_b, io_write_dev_b, io_write_cancelled_b,

/\* /proc/[pid]/oom_score \*/ oom_score,

/\* /proc/[pid]/oom_score_adj \*/ oom_score_adj,

/\* path of /proc/[pid]/root \*/ root,

/\* /proc/[pid]/stat \*/ stat_pid, stat_comm, stat_state, stat_ppid,
stat_pgrp, stat_session, stat_tty_nr, stat_tpgid, stat_flags,
stat_minflt, stat_cminflt, stat_majflt, stat_cmajflt, stat_utime,
stat_stime, stat_cutime, stat_cstime, stat_priority, stat_nice,
stat_num_threads, stat_itrealvalue, stat_starttime, stat_vsize,
stat_rss, stat_rsslim, stat_startcode, stat_endcode, stat_startstack,
stat_kstkesp, stat_kstkeip, stat_signal, stat_blocked, stat_sigignore,
stat_sigcatch, stat_wchan, stat_nswap, stat_cnswap, stat_exit_signal,
stat_processor, stat_rt_priority, stat_policy,
stat_delayacct_blkio_ticks, stat_guest_time, stat_cguest_time,
stat_start_data, stat_end_data, stat_start_brk, stat_arg_start,
stat_arg_end, stat_env_start, stat_env_end, stat_exit_code,

/\* from /proc/[pid]/status \*/ status_name, status_umask, status_state,
status_tgid, status_ngid, status_pid, status_ppid, status_tracerpid,
status_uid, status_real_user, status_eff_user, status_sav_user,
status_fs_user, status_gid, status_real_group, status_eff_group,
status_sav_group, status_fs_group, status_fdsize, status_groups,
status_nstgid, status_nspid, status_nspgid, status_nssid, status_vmpeak,
status_vmsize, status_vmlck, status_vmpin, status_vmhwm, status_vmrss,
status_rssanon, status_rssfile, status_rssshmem, status_vmdata,
status_vmstk, status_vmexe, status_vmlib, status_vmpte, status_vmpmd,
status_vmswap, status_hugetlbpages, status_coredumping, status_threads,
status_sig_queued, status_sig_limit, status_sigpnd, status_shdpnd,
status_sigblk, status_sigign, status_sigcgt, status_capinh,
status_capprm, status_capeff, status_capbnd, status_capamb,
status_nonewprivs, status_seccomp, status_speculation_store_bypass,
status_cpus_allowed, status_cpus_allowed_list, status_mems_allowed,
status_mems_allowed_list, status_voluntary_ctxt_switches,
status_nonvoluntary_ctxt_switches,

/\* /proc/[pid]/syscall \*/ syscall,

/\* /proc/[pid]/timerslack_ns \*/ timerslack_ns,

/\* /proc/[pid]/wchan \*/ wchan, .fi .RE .PP .PP .SH “BUGS” .PP .PP No
known bugs&. .PP .SH “EXAMPLES” .PP .PP .SS “Example 1” .PP Get
everyting: .PP .RS 4 .nf config name=app_sampler

.fi .RE .PP .PP .SS “Example 2” .PP Down-select and with non-default
stream name: .PP .RS 4 .nf config name=app_sampler
metrics=stat_pid,stat_utime stream=mystream

.fi .RE .PP .PP .SS “Example 3” .PP Down-select using config file, using
default stream: .PP .RS 4 .nf config name=app_sampler cfg_file=cfg&.json

.fi .RE .PP .PP .PP .RS 4 .nf # cfg&.json { “metrics” : [ “stat_pid”,
“stat_utime” ] } .fi .RE .PP .PP .SH NOTES

Some of the optionally collected data might be security sensitive.

The status_uid and status_gid values can alternatively be collected as
“status_real_user”, “status_eff_user”, “status_sav_user”,
“status_fs_user”, “status_real_group”, “status_eff_group”,
“status_sav_group”, “status_fs_group”. These string values are most
efficiently collected if both the string value and the numeric values
are collected.

.SH SEE ALSO .nh .BR ldmsd (8), .BR ldms_quickstart (7), .BR
ldmsd_controller (8), .BR ldms_sampler_base (7), .BR proc(5), .BR
sysconf(3), .BR environ(3).
