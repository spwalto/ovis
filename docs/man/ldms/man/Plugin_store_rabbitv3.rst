." Manpage for Plugin_store_rabbitv3 ." Contact ovis-help@ca.sandia.gov
to correct errors or typos. .TH man 7 “03 Dec 2016” “v3” “LDMS Plugin
store_rabbitv3 man page”

.SH NAME Plugin_store_rabbitv3 - man page for the LDMS store_rabbitv3
plugin

.SH SYNOPSIS Within ldmsd_controller or in a configuration file .br load
name=store_rabbitv3 .br config name=store_rabbitv3 [ = ] .br strgp_add
name=store_rabbitv3 [ = ]

.SH DESCRIPTION The store_rabbitv3 plugin is a rabbitmq producer. Actual
storage of data must be arranged separately by configuring some other
amqp client. .PP

.SH CONFIGURATION ATTRIBUTE SYNTAX

The configuration parameters root, host, port, exchange, vhost, user,
and pwfile are shared across all metric sets.

.TP .BR config name= root= host= port= exchange= vhost= user= pwfile=
extraprops= metainterval= .br These parameters are: .RS .TP name= .br
This MUST be store_rabbitv3. .TP root= .br The routing key prefix shared
by all metric sets will be . .TP host= .br The rabbitmq server host. The
default is localhost. .TP port= .br The server port on the nearest
rabbitmq host. The default is 5672. .TP exchange= .br The amqp exchange
to publish with is . The default is amq.topic. .TP vhost= .br The
virtual host to be used is . The default is “/”. .TP user= .br The amqp
username is . The default is “guest”. .TP pwfile= .br The file contains
the amqp user password in the format ’secretword=password. The default
password “guest” is assumed if no file is specified. .TP extraprops= .br
Turn on (y) or off (n) the use of extra properties with all messages.
.TP mint .br The number of seconds between emission of time and host
invariant (meta) metrics. .RE

.SH STORE ATTRIBUTE SYNTAX

.TP .BR store name= schema= container= .br .RS .TP name= .br This MUST
be store_rabbitv3. .TP schema= .br The name of the metric group,
independent of the host name. .TP container= .br The container will be
used in the routing key. The current routing key patterns is: ....

Use a unique container parameter for different metric sets coming from
different sampler (e.g., do not use the same container for procstat and
meminfo); however, use the same container for the same metric set coming
from all hosts (e.g., for all meminfo).

.RE

.SH AMQ event contents

This store generates rabbitmq events. The message in each event is just
the metric value in string form. The message properties of each event
encode everything else. .PP The properties follow the AMQP standard,
with LDMS specific interpretations: .RS .TP timestamp .br The sample
collection time in MICROSECONDS UTC. Divide by 1,000,000 to get seconds
UTC. .TP type .br The ldms metric data type. .TP app_id .br The app_id
is the integer component_id, if it has been defined by the sampler. .SH
Optional AMQ event contents These fields and headers are present if
extraprops=y is configured. .TP content_type .br <“text/plain”> for all.
.TP reply_to .br The producer name. .TP metric .br The label registered
by the sampler plugin, which might be anything. .TP metric_name_amqp .br
The label modified to work as a routing key, not necessarily easily
read. .TP metric_name_least .br The label modified to work as a
programming variable name, possibly shortened and including a hash
suffix. Not expected to be fully human-readable in all cases. It will be
the same across runs for metric sets whose content labels do not vary
across runs. .TP container .br The container configuration name. .TP
schema .br The schema configuration name. .RE

.SH PAYLOAD FORMAT

Payloads are ASCII formatted. .PP Scalar values are formatted in obvious
C ways to ensure full precision is retained. Each is a doublet:
type,value .PP Array values are formatted as comma separated lists:
type,array-length,value[,value]*. .PP Char array values omit the commas
in the value list, giving the appearance of a string. Note however that
there may be embedded nul characters.

.SH NOTES .PP The semantics of LDMS messages are not an extremely close
match to network mail and news messages. The interpretations on message
properties used here may be subject to change in major releases of LDMS.
.PP The authentication to AMQP server uses the SASL plaintext method. In
HPC environments this is normally secure. Additional options enabling
encryption are likely to appear in future work at a cost in CPU.
Normally, an amqp server federation member should be hosted on or very
near the LDMS aggregator host.

.SH BUGS .PP The periodic emission of meta metrics should be per
(producer,metric) pair, but the store API is not yet sufficient to make
this a scalable and efficient operation. In the meanwhile, meta metrics
are emitted on first definition and assumed to be identical for a metric
set across all producers. The special case of component_id (if present)
is handled correctly when extraprops=y is configured.

.SH EXAMPLES .PP See the LDMS test script ldms_local_amqptest.sh.

.SH SEE ALSO ldmsd(8), rabbitmq-server(1), ldmsd_controller(8)
