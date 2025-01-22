.. role:: raw-latex(raw)
   :format: latex
..

.TH man 7 “30 Mar 2023” “v4” “LDMSD Plugin store_avro_kafka man page”

.ad l .nh

.SH “NAME” .PP .PP store_avro_kafka - LDMSD store_avro_kafka plugin .PP
.SH “SYNOPSIS” .SY config .BR name=store_avro_kafka .BI
producer=PRODUCER .BI instance=INSTANCE .OP
:raw-latex:`\fBtopic`=:raw-latex:`\fITOPIC`\_FMT:raw-latex:`\fR` .OP
:raw-latex:`\fBencoding`=:raw-latex:`\fIJSON`:raw-latex:`\fR` .OP
:raw-latex:`\fBencoding`=:raw-latex:`\fIAVRO`:raw-latex:`\fR` .OP
:raw-latex:`\fBkafka`\_conf=:raw-latex:`\fIPATH`:raw-latex:`\fR` .OP
:raw-latex:`\fBserdes`\_conf=:raw-latex:`\fIPATH`:raw-latex:`\fR` .YS
.PP .SH “DESCRIPTION” .PP
:raw-latex:`\f[`CB]store_avro_kafka:raw-latex:`\fR `implements a
decomposition capable LDMS metric data store. The
:raw-latex:`\f[`CB]store_avro_kafka:raw-latex:`\fR `plugin does not
implement the :raw-latex:`\f[`CB]store:raw-latex:`\fR `function and must
only be used with decomposition. .PP The plugin operates in one of two
modes: :raw-latex:`\fIJSON`:raw-latex:`\fR`, and
:raw-latex:`\fIAVRO`:raw-latex:`\fR `(the default). In
:raw-latex:`\fIJSON`:raw-latex:`\fR `mode, each row is encoded as a JSON
formatted text string. In :raw-latex:`\fIAVRO`:raw-latex:`\fR `mode,
each row is associated with an AVRO schema and serialized using an AVRO
Serdes. .PP When in :raw-latex:`\fIAVRO`:raw-latex:`\fR `mode, the
plugin manages schema in cooperation with an Avro Schema Registry. The
location of this registry is specified in a configuration file or
optionally on the :raw-latex:`\f[`CB]config:raw-latex:`\fR `command
line. .PP .SH “CONFIG OPTIONS” .PP .IP
“:raw-latex:`\fBmode `:raw-latex:`\fR`” 1c A string indicating the
encoding mode: “JSON” will encode messages in JSON format, “AVRO” will
encode messages using a schema and Avro Serdes. The default is “AVRO”.
The mode values are not case sensitive. .IP
“:raw-latex:`\fBname `:raw-latex:`\fR`” 1c Must be store_avro_kafka. .IP
":raw-latex:`\fBkafka`\_conf :raw-latex:`\fR`" 1c A path to a
configuration file in Java property format. This configuration file is
parsed and used to configure the Kafka kafka_conf_t configuration
object. The format of this file and the supported attributes are
available here:
https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md. .IP
":raw-latex:`\fBserdes`\_conf :raw-latex:`\fR`" 1c A path to a
configuration file in Java property format. This configuration file is
parsed and used to configure the Avro Serdes serdes_conf_t configuration
object. The only supported option for this file is serdes.schema.url.

.SH “TOPIC NAMES” .PP The topic name to which messages are published is
defined by the :raw-latex:`\f[`BR]topic:raw-latex:`\fR `configuration
parameter. The parameter specifies a string that is a
:raw-latex:`\fIformat `specifier:raw-latex:`\fR `similar to a printf()
format string. If the :raw-latex:`\f[`BR]topic:raw-latex:`\fR `is not
specified, it defaults to “%S” which is the format specifier for the set
schema name. .PP The ‘%’ character introduces a
:raw-latex:`\fIformat `specifier:raw-latex:`\fR `that will be
substituted in the topic format string to create the topic name. The
format specifiers are as follows: .IP “:raw-latex:`\fB`%F
:raw-latex:`\fR`” 1c The format in which the message is serialized:
“json” or “avro”. .IP “:raw-latex:`\fB`%S :raw-latex:`\fR`” 1c The set
parameter’s :raw-latex:`\fIschema`:raw-latex:`\fR `name. .IP
“:raw-latex:`\fB`%I :raw-latex:`\fR`” 1c The instance name of the set,
e.g. “orion-01/meminfo”. .IP “:raw-latex:`\fB`%P :raw-latex:`\fR`” 1c
The set parameter’s :raw-latex:`\fIproducer`:raw-latex:`\fR `name,
e.g. “orion-01.” .IP “:raw-latex:`\fB`%u :raw-latex:`\fR`” 1c The
user-name string for the owner of the set. If the user-name is not known
on the system, the user-id is used. .IP “:raw-latex:`\fB`%U
:raw-latex:`\fR`” 1c The user-id (uid_t) for the owner of the set. .IP
“:raw-latex:`\fB`%g :raw-latex:`\fR`” 1c The group-name string for the
group of the set. If the group-name is not known on the system, the
group-id is used. .IP “:raw-latex:`\fB`%G :raw-latex:`\fR`” 1c The
group-id (gid_t) for the group of the set. .IP “:raw-latex:`\fB`%a
:raw-latex:`\fR`” 1c The access/permission bits for the set formatted as
a string, e.g. “-rw-rw—-”. .IP “:raw-latex:`\fB`%A :raw-latex:`\fR`” 1c
The access/permission bits for the set formatted as an octal number,
e.g. 0440. .PP Note that a topic name must only consist of a combination
of the characters [a-zA-Z0-9\._\-]. In order to ensure that the format
specifier above will not produce invalid topic names, any character that
results from a format specifier substitution that is not in the valid
list will be substituted with a ‘.’. .PP .SH “STRGP” .PP The
store_avro_kafka is used with a storage policy that specifies
store_avro_kafka as the plugin parameter. .PP The
:raw-latex:`\fIschema`:raw-latex:`\fR`,
:raw-latex:`\fIinstance`:raw-latex:`\fR`,
:raw-latex:`\fIproducer`:raw-latex:`\fR `and
:raw-latex:`\fIflush`:raw-latex:`\fR `strgp_add parameters have no
affect on how data is stored. If the
:raw-latex:`\fIcontainer`:raw-latex:`\fR `parameter is set to any value
other than an empty string, it will override the bootstrap.servers Kafka
configuration parameter in the kafka_conf file if present. .PP .SH “JSON
Mode” .PP JSON mode encodes messages as self describing text objects.
Each message is a JSON dictionary based on the following template: RS 4
.nf { “” : , “” : , … } .fi .RE .PP Each row in the decomposition is
encoded as shown. The :raw-latex:`\fBattr`-value:raw-latex:`\fR `types
are mapped to either quoted strings, floating-point, or integers as
defined by the source metric type in the LDMS metric set. The mapping is
as follows: .TS tab(@) allbox; l l l .
:raw-latex:`\fBMetric `Type:raw-latex:`\fR@`:raw-latex:`\fBFormat `Specifier:raw-latex:`\fR@`:raw-latex:`\fBDescription`:raw-latex:`\fR`
LDMS_V_TIMESTAMP@%u.%06u@Floating point number in seconds
LDMS_V_CHAR@%s@String LDMS_V_U8@%hhu@Unsigned integer
LDMS_V_S8@%hhd@Signed integer LDMS_V_U16@%hu@Unsigned integer
LDMS_V_S16@%hd@Signed integer LDMS_V_U32@%u@Unsigned integer
LDMS_V_S32@%d@Signed integer LDMS_V_U64@%lu@Unsigned integer
LDMS_V_S64@%ld@Signed integer LDMS_V_FLOAT@%.9g@Floating point
LDMS_V_DOUBLE@%.17g@Floating point LDMS_V_STRING@“%s”@Double quoted
string LDMS_V_ARRAY_xxx@[ v0, v1, … ]@Comma separated value list
surrounding by ‘[]’ .TE .SS “Example JSON Object”
{“timestamp”:1679682808.001751,“component_id”:8,“dev_name”:“veth1709f8b”,“rx_packets”:0,“rx_err_packets”:0,“rx_drop_packets”:0,“tx_packets”:858,“tx_err_packets”:0,“tx_drop_packets”:0}
.fi .RE .PP .SH “Avro Mode” .PP In Avro mode, LDMS metric set values are
first converted to Avro values. The table below describes how each LDMS
metric set value is represented by an Avro value. .PP Each row in the
decomposition is encoded as a sequence of Avro values. The target Avro
type is governed by the Avro schema. The mapping is as follows: .TS
tab(@) allbox; l l l .
:raw-latex:`\fBMetric `Type:raw-latex:`\fR@`:raw-latex:`\fBAvro `Type:raw-latex:`\fR@`:raw-latex:`\fBLogicalType`:raw-latex:`\fR`
LDMS_V_TIMESTAMP@AVRO_INT64@timestamp-millis
LDMS_V_CHAR@AVRO_STRING@single-character LDMS_V_U8@AVRO_INT32@uint8
LDMS_V_S8@AVRO_INT32@int8 LDMS_V_U16@AVRO_INT32@unsigned-short
LDMS_V_S16@AVRO_INT32@signed-short LDMS_V_U32@AVRO_INT64@unsigned-int
LDMS_V_S32@AVRO_INT32@ LDMS_V_U64@AVRO_INT64@unsigned-long
LDMS_V_S64@AVRO_INT64@ LDMS_V_FLOAT@AVRO_FLOAT@
LDMS_V_DOUBLE@AVRO_DOUBLE@ LDMS_V_CHAR_ARRAY@AVRO_STRING@
LDMS_V_ARRAY_xxx@AVRO_ARRAY@Comma separated value list or primitive type
surrounded by ‘[]’ .TE .SS “Schema Creation” .PP Each row in the LDMS
metric set presented for storage is used to generate an Avro schema
definition. The table above shows the Avro types that are used to store
each LDMS metric type. Note that currently, all LDMS_V_TIMESTAMP values
in a metric set are stored as the Avro logical type “timestamp-millis”
and encoded as an Avro long. .PP Unsigned types are currently encoded as
signed types. The case that could cause issues is LDMS_V_U64 which when
encoded as AVRO_LONG will result in a negative number. One way to deal
with this is to encode these as AVRO_BYTES[8] and let the consumer
perform the appropriate cast. This, however, seems identical to simply
encoding it as a signed long and allow the consumer to cast the signed
long to an unsigned long. .SS “Schema Registration” .PP The Avro schema
are generated from the row instances presented to the commit() storage
strategy routine. The :raw-latex:`\fBschema`\_name:raw-latex:`\fR `that
is contained in the row instance is used to search for a serdes schema.
This name is first searched for in a local RBT and if not found, the
Avro Schema Registry is consulted. If the schema is not present in the
registry, a new Avro schema is constructed per the table above,
registered with the schema registry and stored in the local cache.

Note that Avro schema names must contain only the characters
[a-zA-Z0-9\._\-], any characters in the row schema name that do not come
from this set will be forced to ‘.’.

A similar mapping is done for Avro value names, however, because these
names cannot accept the character ‘.’, all invalid characters are mapped
to ’_’.

These change are made automatically and no errors are generated. .PP .SS
“Encoding” .PP After the schema is located, constructed, and or
registered for the row, the schema in conjunction with libserdes is used
to binary encode the Avro values for each column in the row. Once
encoded, the message is submitted to Kafka. .SS “Client Side Decoding”
.PP Consumers of topics encoded with libserdes will need to perform the
above procedure in reverse. The message received via Kafka will have the
schema-id present in the message header. The client will use this
schema-id to query the Schema registry for a schema. Once found, the
client will construct a serdes from the schema definition and use this
serdes to decode the message into Avro values. .SH “EXAMPLES” .PP .PP
.SS “kafka_conf Example File” .PP .RS 4 .nf # Lines beginning with ‘#’
are considered comments. # Comments and blank lines are ignored.

Specify the location of the Kafka broker
========================================

bootstrap.server=localhost:9092 .fi .RE .PP .SS “serdes_conf Example
File” .PP .RS 4 .nf # Specify the location of the Avro Schema registry.
This can be overridden # on the strgp_add line with the “container”
strgp_add option if it is # set to anything other than an empty string
serdes.schema.url=https://localhost:8081 .fi .RE .PP .SS “Example
strg_add command” .PP .RS 4 .nf strgp_add name=aks
plugin=store_avro_kafka container=kafka-broker.int:9092
decomposition=aks-decomp.conf strgp_start name=aks .fi .RE .PP .SS
“Example strg_add command w/o container” .PP In this example, the
strgp_add parameter, container, is set to be ignored by
store_avro_kafka. In this case, either the default, localhost:9092, or
the value specified in the rd_kafka_conf file is used. .RS 4 .nf
strgp_add name=aks plugin=store_avro_kafka container=
decomposition=aks-decomp.conf strgp_start name=aks .fi .RE .PP .SS
“Example plugin configuration” .PP .RS 4 .nf config
name=store_avro_kafka encoding=avro kafka_conf=/etc/kakfa.conf
serdes_conf=/etc/serdes.conf topic=ldms.%S strgp_start name=aks .fi .RE
.PP .SH NOTES .PP This man page is a work in progress. .SH SEE ALSO .nh
.BR ldmsd (8), .BR ldmsd_controller (8), .BR ldmsd_decomposition (7),
.BR ldms_quickstart (7)
