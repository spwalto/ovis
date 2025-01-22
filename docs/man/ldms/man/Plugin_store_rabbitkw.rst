." Manpage for Plugin_store_rabbitkw ." Contact ovis-help@ca.sandia.gov
to correct errors or typos. .TH man 7 “10 Jun 2018” “kw” “LDMS Plugin
store_rabbitkw man page”

.SH NAME Plugin_store_rabbitkw - man page for the LDMS store_rabbitkw
plugin

.SH SYNOPSIS Within ldmsd_controller or in a configuration file .br load
name=store_rabbitkw .br config name=store_rabbitkw [ = ] .br strgp_add
name=store_rabbitkw [ = ]

.SH DESCRIPTION The store_rabbitkw plugin is a rabbitmq producer. Actual
storage of data must be arranged separately by configuring some other
amqp client. .PP

.SH CONFIGURATION ATTRIBUTE SYNTAX

The configuration parameters routing_key, host, port, exchange, vhost,
user, and pwfile are shared across all metric sets.

.TP .BR config name= exchange= routing_key= host= port= vhost= user=
pwfile= [extraprops= logmsg= useserver=[y/n> heartbeat= timeout= retry=]
.br These parameters are: .RS .TP name= .br This MUST be store_rabbitkw.
.TP routing_key .br The routing key shared by all metric sets is . .TP
host= .br The rabbitmq server host. The default is localhost. .TP port=
.br The server port on the nearest rabbitmq host. The default is 5672.
.TP exchange= .br The amqp exchange to publish with is . The default is
amq.topic. This must preexist; the plugin will no cause its creation.
.TP vhost= .br The virtual host to be used is . The default is “/”. .TP
user= .br The amqp username is . The default is “guest”. .TP pwfile= .br
The file contains the amqp user password in the format
‘secretword=password. The default password “guest” is assumed if no file
is specified. .TP retry= .br If amqp connection fails due to network or
server issue, retry every seconds. Default is 60. .TP heartbeat= .br
Heartbeat interval used to detect failed connections. .TP timeout= .br
Timeout to use for connections, in milliseconds. Default is 1000. .TP
extraprops= .br Turn on (y) or off (n) the use of extra properties with
all messages. If AMQP-based filtering is not planned, ’n’ will reduce
message sizes slightly. .TP logmsg= .br Enable (y) or disable (n, the
default) logging all message metric content at the DEBUG level. This is
a debugging option. .TP useserver= .br Enable (y, the default) or
disable (n) calls to the amqp server; this is a debugging option. .RE

.SH STORE ATTRIBUTE SYNTAX

.TP .BR store name= schema= container= .br .RS .TP name= .br This MUST
be store_rabbitkw. .TP schema= .br The name of the metric group,
independent of the host name. The schema will be used as a header in
messages if extraprops is y. .TP container= .br The container will be
used as a header in messages if extraprops is y.

.RE

.SH AMQ event contents

This store generates rabbitmq events containing the data from LDMS set
instances. All events are on the single queue that is configured. .PP
The properties follow the AMQP standard, with LDMS specific
interpretations: .RS .TP timestamp .br The sample collection time in
MICROSECONDS UTC. Divide by 1,000,000 to get seconds UTC. .TP app_id .br
The app_id is LDMS. .SH Optional AMQ event contents These fields and
headers are present if extraprops=y is configured. .TP content_type .br
<“text/plain”> for all. .TP reply_to .br The metric set instance name.
.TP container .br The container configuration name. .TP schema .br The
schema configuration name. .RE

.SH PAYLOAD FORMAT

Payloads are ASCII formatted, tab separated “label=val” lists. .PP
Scalar metric values are formatted in obvious C ways to ensure full
precision is retained. Each is a tab-separated triplet
‘metric=\ :math:`name type=`\ scalar_type
value=\ :math:`value'. Before the metric values on each line are the keys and values: timestamp_us, producer, container, schema. .PP Array values are formatted as semicolon separated lists: Each metric appears as a tab-separated quartet 'metric=`\ name
type=\ :math:`scalar_type length=`\ array_length value=$value’. .PP
CHAR_ARRAY values are formatted as strings. Note these are terminated at
the first nul character.

.SH NOTES .PP .PP The semantics of LDMS messages are not an extremely
close match to network mail and news messages targeted by AMQP. The
interpretations on message properties used here may be subject to change
in future releases. .PP The authentication to AMQP server uses the SASL
plaintext method. In HPC environments this is normally secure.
Additional options enabling encryption are likely to appear in future
work at a cost in CPU. Normally, an amqp server federation member should
be hosted on or very near the LDMS aggregator host. .PP Presently each
payload contains a single line (with tab separators). Future versions
may capture multiple set instances per message, where each set is
separated by newlines from the others. .PP The behavior of this AMQP
client when faced with AMQP server disappearance is to retry connection
later and to ignore any metric data seen while disconnected.

.SH BUGS .PP String data containing tab characters are not compatible
with this data encoding. This may be fixed when a satisfactory alternate
representation is agreed for these special characters.

.SH EXAMPLES .PP See the LDMS test script rabbitkw

.SH ADMIN HINTS .PP On Linux, this requires an amqp service (typically
rabbitmq-server.service) running in the network. That service may
require epmd.service.

.SH SEE ALSO ldmsd(8), rabbitmq-server(1), ldmsd_controller(8),
store_rabbitv3(7)
