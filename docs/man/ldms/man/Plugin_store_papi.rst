.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for store_papi ." Contact ovis-help@ca.sandia.gov to correct
errors or typos. .TH man 7 “30 Sep 2019” “v4” “LDMSD Plugin store_papi
man page”

###############################################################################
.SH NAME

Plugin_store_papi - man page for the LDMSD store_papi plugin

###############################################################################
.SH SYNOPSIS

Within ldmsd_controller or a configuration file:

.SY load .BI name=store_papi

.SY config .BI name=store_papi .BI path= STORE_ROOT_PATH

.SY strgp_add .BI name= STRGP_NAME .BI plugin=store_papi .BI container=
CONTAINER .BI schema= SCHEMA

.SY strgp_prdcr_add .BI name= STRGP_NAME .BI regex= PRDCR_REGEX .YS

###############################################################################
.SH DESCRIPTION

:raw-latex:`\fBstore`\_papi:raw-latex:`\fR `is an LDMSD storage plugin
for storing data from
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `specifically as it expects
a collection of PAPI event metrics after a certain job metric
(task_ranks) that only :raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR`
produced. :raw-latex:`\fBstore`\_papi:raw-latex:`\fR `stores data in a
SOS container (specified by
:raw-latex:`\fBstrgp`:raw-latex:`\fR `:raw-latex:`\fBcontainer`:raw-latex:`\fR `option).
Unlike :raw-latex:`\fBstore`\_sos:raw-latex:`\fR `(see
:raw-latex:`\fBPlugin`\_store_sos:raw-latex:`\fR`(7)) where an entire
LDMS snapshot results in an SOS data entry,
:raw-latex:`\fBstore`\_papi:raw-latex:`\fR `split the PAPI events in the
set into their own schemas and data points. For example, if we have
PAPI_TOT_INS and PAPI_TOT_CYC as PAPI events in the
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `set, we will have
PAPI_TOT_INS and PAPI_TOT_CYC schemas in the SOS container storing
respective PAPI events. This allows storing flexible, user-defined
schemas at run-time by user jobs (LDMS schemas of sets from
:raw-latex:`\fBpapi`\_sampler:raw-latex:`\fR `are defined at run-time by
user jobs). Please note that the schema name defined by user job must
match :raw-latex:`\fBstrgp`:raw-latex:`\fR`’s schema in order to store
the data.

###############################################################################
.SH CONFIG OPTIONS

.TP .BR name=store_papi This MUST be store_papi (the name of the
plugin). .TP .BI path= STORE_ROOT_PATH The path to the root of the
store. SOS container for each schema specified by the storage policy
(:raw-latex:`\fBstrgp`:raw-latex:`\fR`) will be placed in the
:raw-latex:`\fISTORE`\_ROOT_PATH:raw-latex:`\fR` directory.

###############################################################################
.SH STORAGE POLICY

An LDMSD storage plugin is like a storage driver that provides only
storing mechanism. A storage policy
(:raw-latex:`\fBstrgp`:raw-latex:`\fR`) is a glue binding data sets from
various producers to a container of a storage plugin.

:raw-latex:`\fBstrgp`\_add:raw-latex:`\fR `command defines a new storage
policy, identified by :raw-latex:`\fBname`:raw-latex:`\fR`. The
:raw-latex:`\fBplugin`:raw-latex:`\fR `attribute tells the storage
policy which storage plugin to work with. The
:raw-latex:`\fBschema`:raw-latex:`\fR `attribute identifies LDMS schema
the data set of which is consumed by the storage policy. The
:raw-latex:`\fBcontainer`:raw-latex:`\fR `attribute identifies a
container inside the storage plugin that will store data.

:raw-latex:`\fBstrgp`\_prdcr_add:raw-latex:`\fR `is a command to specify
producers that feed data to the storage policy.

###############################################################################
.SH BUGS

No known bugs.

###############################################################################
.SH EXAMPLES

Plugin configuration example:

.RS .EX load name=store_papi config name=store_papi path=/var/store
strgp_add name=papi_strgp plugin=store_papi container=papi schema=papi
strgp_prdcr_add name=papi_strgp regex=.\* .EE .RE

The following job script and PAPI JSON config combination is an example
of submiting a PAPI-enabled job that will end up in the storage of the
configuration above.

Job script example:

.RS .EX #!/bin/bash export
SUBSCRIBER_DATA=‘{“papi_sampler”:{“file”:“/tmp/papi.json”}}’ srun bash
-c ‘for X in {1..60}; do echo $X; sleep 1; done’ .EE .RE

PAPI JSON example (/tmp/papi.json):

.RS .EX { “schema”: “papi”, “events”: [ “PAPI_TOT_INS”, “PAPI_L1_DCM” ]
} .EE .RE

###############################################################################
.SH SEE ALSO

.nh .BR Plugin_papi_sampler (7), .BR ldmsd (8), .BR ldms_quickstart (7),
.BR ldmsd_controller (8), .BR ldms_sampler_base (7).
