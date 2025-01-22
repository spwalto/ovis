.. role:: raw-latex(raw)
   :format: latex
..

.TH man 7 “30 Sep 2019” “v4” “LDMSD Plugin store_app man page”

.ad l .nh

.SH “NAME” .PP .PP ldmsd_store_app - LDMSD store_app storage plugin .PP
.SH “SYNOPSIS” .PP .SY load
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fBstore`\_app:raw-latex:`\fR`
.PP .SY :raw-latex:`\fBconfig`:raw-latex:`\fR`
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fBstore`\_app:raw-latex:`\fR `:raw-latex:`\fBpath`:raw-latex:`\fR`=:raw-latex:`\fISTORE`\_ROOT_PATH:raw-latex:`\fR`
.OP
:raw-latex:`\fBperm`:raw-latex:`\fR`=:raw-latex:`\fIOCTAL`\_PERM:raw-latex:`\fR`
.PP .SY :raw-latex:`\fBstrgp`\_add:raw-latex:`\fR`
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fISTRGP`\_NAME:raw-latex:`\fR `:raw-latex:`\fBplugin`:raw-latex:`\fR`=:raw-latex:`\fBstore`\_app:raw-latex:`\fR `:raw-latex:`\fBcontainer`:raw-latex:`\fR`=:raw-latex:`\fICONTAINER`\_NAME:raw-latex:`\fR `:raw-latex:`\fBschema`:raw-latex:`\fR`=:raw-latex:`\fILDMS`\_SCHEMA:raw-latex:`\fR`
.PP .SY :raw-latex:`\fBstrgp`\_prdcr_add:raw-latex:`\fR`
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fISTRGP`\_NAME:raw-latex:`\fR `:raw-latex:`\fBregex`:raw-latex:`\fR`=:raw-latex:`\fIPRDCR`\_REGEX:raw-latex:`\fR`
.YS .SH “DESCRIPTION” .PP .PP
:raw-latex:`\f[`CB]store_app:raw-latex:`\fR `is an LDMSD storage plugin
for storing data from the sets from
:raw-latex:`\f[`CB]app_sampler:raw-latex:`\fR `LDMSD sampler plugin&.
:raw-latex:`\f[`CB]store_app:raw-latex:`\fR `uses
:raw-latex:`\f[`CB]SOS:raw-latex:`\fR `as its database back-end&. The
:raw-latex:`\f[`CB]path:raw-latex:`\fR `option points to the directory
containing :raw-latex:`\f[`CB]SOS:raw-latex:`\fR `containers for this
plugin (one container per :raw-latex:`\f[`CB]strgp:raw-latex:`\fR`)&. If
the container does not exist, it will be created with permission given
by :raw-latex:`\f[`CB]perm:raw-latex:`\fR `option (default: 0660)&. The
container contains multiple schemas, each of which assoicates with a
metric from the sets from
:raw-latex:`\f[`CB]app_sampler:raw-latex:`\fR `(e&.g&.
:raw-latex:`\f[`CB]stat_utime:raw-latex:`\fR`)&. Schemas in the
container have the following attributes: .PP .IP “(bu” 2
:raw-latex:`\f[`CB]timestamp:raw-latex:`\fR `: the data sampling
timestamp&. .IP “(bu” 2 :raw-latex:`\f[`CB]component_id:raw-latex:`\fR`:
the component ID producing the data&. .IP “(bu” 2
:raw-latex:`\f[`CB]job_id:raw-latex:`\fR`: the Slurm job ID&. .IP “(bu”
2 :raw-latex:`\f[`CB]app_id:raw-latex:`\fR`: the application ID&. .IP
“(bu” 2 :raw-latex:`\f[`CB]rank:raw-latex:`\fR`: the Slurm task rank&.
.IP “(bu” 2 :raw-latex:`\f[`BI]METRIC_NAME:raw-latex:`\fR`: the metric
value (the name of this attribute is the metric name of the metric)&.
.IP “(bu” 2 :raw-latex:`\f[`CB]comp_time:raw-latex:`\fR`: (indexed) the
join of :raw-latex:`\f[`CB]component_id:raw-latex:`\fR `and
:raw-latex:`\f[`CB]timestamp:raw-latex:`\fR`&. .IP “(bu” 2
:raw-latex:`\f[`CB]time_job:raw-latex:`\fR`: (indexed) the join of
:raw-latex:`\f[`CB]timestamp:raw-latex:`\fR `and
:raw-latex:`\f[`CB]job_id:raw-latex:`\fR`&. .IP “(bu” 2
:raw-latex:`\f[`CB]job_rank_time:raw-latex:`\fR`: (indexed) the join of
:raw-latex:`\f[`CB]job_id:raw-latex:`\fR`,
:raw-latex:`\f[`CB]rank:raw-latex:`\fR`, and
:raw-latex:`\f[`CB]timestamp:raw-latex:`\fR`&. .IP “(bu” 2
:raw-latex:`\f[`CB]job_time_rank:raw-latex:`\fR`: (indexed) the join of
:raw-latex:`\f[`CB]job_id:raw-latex:`\fR`,
:raw-latex:`\f[`CB]timestamp:raw-latex:`\fR`, and
:raw-latex:`\f[`CB]rank:raw-latex:`\fR`&. .PP .PP .SH “CONFIG OPTIONS”
.PP .PP .IP “:raw-latex:`\fBname `:raw-latex:`\fR`” 1c The name of the
plugin instance to configure&. .IP
“:raw-latex:`\fBpath `:raw-latex:`\fR`” 1c The path to the directory
that contains SOS containers (one container per strgp)&. .IP
“:raw-latex:`\fBperm `:raw-latex:`\fR`” 1c The octal mode (e&.g&. 0777)
that is used in SOS container creation&. The default is
:raw-latex:`\fB0660`:raw-latex:`\fR`&. .PP .PP .SH “EXAMPLES” .PP .PP
.PP .RS 4 .nf # in ldmsd config file load name=store_app config
name=store_app path=/sos perm=0600 strgp_add name=app_strgp
plugin=mstore_app container=app schema=app_sampler # NOTE: the schema in
strgp is LDMS set schema, not to confuse with the one # schema per
metric in our SOS container&. strgp_prdcr_add name=app_strgp regex=&.\*
strgp_start name=app_strgp .fi .RE .PP .PP The following is an example
on how to retrieve the data using Python: .PP .RS 4 .nf from sosdb
import Sos cont = Sos&.Container() cont&.open(‘/sos/app’) sch =
cont&.schema_by_name(‘status_vmsize’) attr =
sch&.attr_by_name(‘time_job’) # attr to iterate over must be indexed itr
= attr&.attr_iter() b = itr&.begin() while b == True: obj = itr&.item()
print(obj[‘status_vmsize’]) # object attribute access by name
print(obj[5]) # equivalent to above print(obj[:]) # get everything at
once b = itr&.next()

.fi .RE .PP .PP .SH SEE ALSO .nh .BR Plugin_app_sampler (7), .BR ldmsd
(8), .BR ldms_quickstart (7), .BR ldmsd_controller (8),
