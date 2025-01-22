." Manpage for Plugin_opa2 ." Contact ovis-help@ca.sandia.gov to correct
errors or typos. .TH man 7 “5 Feb 2018” “v4.0” “LDMS Plugin opa2 man
page”

.SH NAME Plugin_opa2 - man page for the LDMS opa2 OmniPath network
plugin

.SH SYNOPSIS Within ldmsd_controller or a configuration file: .br load
name=opa2 config name=opa2 [ = ]

.SH DESCRIPTION The opa2 plugin provides local port counters from
OmniPath hardware. A separate data set is created for each port. All
sets use the same schema.

.SH CONFIGURATION ATTRIBUTE SYNTAX

.TP .BR config name= producer= instance= [schema=] [component_id=]
[ports=] .br configuration line .RS .TP name= .br This MUST be opa2. .TP
producer= .br The producer string value. .TP instance= .br The set_name
supplied is ignored, and the name :math:`producer/`\ CA/$port is used.
.TP schema= .br Optional schema name. Default opa2. The same schema is
used for all sets. .TP component_id= .br Optional component identifier.
Defaults to zero. .TP ports= .br Port list is a comma separated list of
ca_name.portnum or a ‘*’. The default is ’*’, which collects a set for
every host fabric interface port. .RE

.SH BUGS None known.

.SH EXAMPLES .PP Within ldmsd_controller or a configuration file: .nf
load name=opa2 config name=opa2 producer=compute1 instance=compute1/opa2
component_id=1 start name=opa2 interval=1000000 .fi

.SH NOTES This sampler will be expanded in the future to capture
additional metrics.

.SH SEE ALSO ldmsd(8), ldms_quickstart(7), ldmsd_controller(8)
