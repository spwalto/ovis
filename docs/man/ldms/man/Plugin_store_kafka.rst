.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for Plugin_store_kafka ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “2 Jun 2022” “v4” “LDMS Plugin
store_kafka man page”

.SH NAME Plugin_store_kafka - man page for the LDMS store_kafka plugin

.SH SYNOPSIS Within ldmsd_controller script: .br ldmsd_controller> load
name=store_kafka .br ldmsd_controller> config name=store_kafka [path=]
.br ldmsd_controller> strgp_add name= plugin=store_kafka container=
decomposition= .br

.SH DESCRIPTION

:raw-latex:`\fBstore`\_kafka:raw-latex:`\fP `uses librdkafka to send
rows from the decomposition to the Kafka servers (specified by strgp’s
:raw-latex:`\fIcontainer`:raw-latex:`\fP `parameter) in JSON format. The
row JSON objects have the following format: { “column_name”:
COLUMN_VALUE, … }.

.SH PLUGIN CONFIGURATION .SY config .BI name= store_kafka .OP
:raw-latex:`\fBpath`=:raw-latex:`\fIKAFKA`\_CONFIG_JSON_FILE:raw-latex:`\fR`
.YS

Configuration Options: .RS .TP .BI name= store_kafka .br The name of the
plugin. This must be :raw-latex:`\fBstore`\_kafka:raw-latex:`\fR`.

.TP .BI path= KAFKA_CONFIG_JSON_FILE The optional KAFKA_CONFIG_JSON_FILE
contains a dictionary with KEYS being Kafka configuration properties and
VALUES being their corresponding values.
:raw-latex:`\fBstore`\_kafka:raw-latex:`\fR `usually does not require
this option. The properties in the KAFKA_CONFIG_JSON_FILE is applied to
all Kafka connections from store_kafka. Please see .UR
:https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
librdkafka CONFIGURATION page .UE for a list of supported properties.
.RE

.SH STRGP CONFIGURATION .SY strgp_add .BI name= NAME .BR plugin=
store_kafka .BI container= KAFKA_SERVER_LIST .BI decomposition=
DECOMP_CONFIG_JSON_FILE .YS

strgp options: .RS .TP .BI name= NAME .br The name of the strgp.

.TP .BR plugin= store_kafka .br The plugin must be store_kafka.

.TP .BI container= KAFKA_SERVER_LIST .br A comma-separated list of Kafka
servers (host[:port]). For example: container=localhost,br1.kf:9898.

.TP .BI decomposition= DECOMP_CONFIG_JSON_FILE .br Set-to-row
decomposition configuration file (JSON format). See more about
decomposition in
:raw-latex:`\fBldmsd`\_decomposition:raw-latex:`\fP`(7).

.RE

.SH SEE ALSO ldmsd_decomposition(7)
