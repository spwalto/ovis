.. role:: raw-latex(raw)
   :format: latex
..

" Manpage for ldmsd_peer_daemon_advertisement .TH man 7 “12 December
2024” “v4” “LDMSD Peer Daemon Advertisement man page”

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH NAME ldmsd_peer_daemon_advertisement - Manual for LDMSD Peer Daemon
Advertisement

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH SYNOPSIS

**Peer side Commands**

.IP :raw-latex:`\fBadvertiser`\_add .RI “name=” NAME " xprt=" XPRT "
host=" HOST " port=" PORT " reconnect=" RECONNECT .RI “[auth="
AUTH_DOMAIN "]”

.IP :raw-latex:`\fBadvertiser`\_start .RI “name=” NAME .RI “[xprt=" XPRT
" host=" HOST " port=" PORT " auth=" AUTH_DOMAIN " reconnect=" RECONNECT
"]”

.IP :raw-latex:`\fBadvertiser`\_stop .RI “name=” NAME

.IP :raw-latex:`\fBadvertiser`\_del .RI “name=” NAME

.IP :raw-latex:`\fBadvertiser`\_status .RI “[name=" NAME "]”

.PP **Aggregator Side Commands**

.IP :raw-latex:`\fBprdcr`\_listen_add .RI “name=” NAME " .RI
“[disable_start=" TURE|FALSE "] [regex=" REGEX "] [ip=" CIDR "]”

.IP :raw-latex:`\fBprdcr`\_listen_start .RI “name=” NAME

.IP :raw-latex:`\fBprdcr`\_listen_stop .RI “name=” NAME

.IP :raw-latex:`\fBprdcr`\_listen_del .RI “name=” NAME

.IP :raw-latex:`\fBprdcr`\_listen_status

.SH DESCRIPTION

LDMSD Peer Daemon Advertisement is a capability that enables LDMSD to
automatically add producers for advertisers whose hostname matches a
regular expression or whose IP address is in a range. The feature
reduces the need for manual configuration of producers in configuration
files.

Admins specify the aggregator’s hostname and listening port in the
peer’s configuration via the
:raw-latex:`\fBadvertiser`\_add:raw-latex:`\fR `command and start the
advertisement with the
:raw-latex:`\fBadvertiser`\_start:raw-latex:`\fR `command. The peer
daemon advertises their hostname to the aggregator. On the aggregator,
admins specify a regular expression to be matched with the peer hostname
or an IP range that the peer IP address falls in via the
:raw-latex:`\fBprdcr`\_listen_add:raw-latex:`\fR `command. The
:raw-latex:`\fBprdcr`\_listen_start:raw-latex:`\fR `command is used to
tell the aggregator to automatically add producers corresponding to a
peer daemon whose hostname matches the regular expression or whose IP
address falls in the IP range. If neither a regular expression nor an IP
range is given, the aggregator will create a producer upon receiving any
advertisement messages.

The auto-generated producers are of the ‘advertised’ type. The producer
name is :raw-latex:`\fB<host:port>`:raw-latex:`\fR`, where
:raw-latex:`\fBhost`:raw-latex:`\fR `is the peer hostname, and
:raw-latex:`\fBport`:raw-latex:`\fR `is the first listening port of the
peer daemon. LDMSD automatically starts the advertised producers, unless
the ‘disable_start’ attribute is given on the
:raw-latex:`\fBprdcr`\_listen_add:raw-latex:`\fR `line. The advertised
producers need to be stopped manually by using the command
:raw-latex:`\fBprdcr`\_stop:raw-latex:`\fR `or
:raw-latex:`\fBprdcr`\_stop_regex:raw-latex:`\fR`. They can be restarted
by using the command :raw-latex:`\fBprdcr`\_start:raw-latex:`\fR `or
:raw-latex:`\fBprdcr`\_start_regex:raw-latex:`\fR`.

The description for each command and its parameters are as follows.

**Peer Side Commands**

:raw-latex:`\fBadvertiser`\_add:raw-latex:`\fR `adds a new
advertisement. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Advertiser
name .IP :raw-latex:`\fBhost`:raw-latex:`\fR`=:raw-latex:`\fIHOST`
Aggregator hostname .IP
:raw-latex:`\fBxprt`:raw-latex:`\fR`=:raw-latex:`\fIXPRT` Transport to
connect to the aggregator .IP
:raw-latex:`\fBport`:raw-latex:`\fR`=:raw-latex:`\fIPORT` Listen port of
the aggregator .IP
:raw-latex:`\fBreconnect`:raw-latex:`\fR`=:raw-latex:`\fIINTERVAL`
Reconnect interval .IP :raw-latex:`\fB[auth\fR=\fIAUTH_DOMAIN\fB]`The
authentication domain to be used to connect to the aggregator .RE

:raw-latex:`\fBadvertiser`\_start:raw-latex:`\fR `starts an
advertisement. If the advertiser does not exist, LDMSD will create the
advertiser. In this case, the mandatory attributes for
:raw-latex:`\fBadvertiser`\_add:raw-latex:`\fB `must be given. The
parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of the
advertiser to be started .IP
:raw-latex:`\fB[host\fR=\fIHOST\fB]`Aggregator hostname .IP
:raw-latex:`\fB[xprt\fR=\fIXPRT\fB]`Transport to connect to the
aggregator .IP :raw-latex:`\fB[port\fR=\fIPORT\fB]`Listen port of the
aggregator .IP :raw-latex:`\fB[reconnect\fR=\fIINTERVAL\fB]`Reconnect
interval .IP :raw-latex:`\fB[auth\fR=\fIAUTH_DOMAIN\fB]`The
authentication domain to be used to connect to the aggregator .RE

:raw-latex:`\fBadvertiser`\_stop:raw-latex:`\fR `stops an advertisement.
The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Nmae of the
advertiser to be stopped .RE

:raw-latex:`\fBadvertiser`\_del:raw-latex:`\fR `deletes an
advertisement. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of the
advertiser to be deleted .RE

:raw-latex:`\fBadvertiser`\_status reports the status of each
advertisement. An optional parameter is: .RS .IP
:raw-latex:`\fB[name\fR=\fINAME\fB]`Advertiser name .RE

.PP **Aggregator Side commands**

:raw-latex:`\fBprdcr`\_listen_add:raw-latex:`\fR `adds a prdcr_listen.
The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` String of the
prdcr_listen name. .IP
:raw-latex:`\fB[disable_start\fR=\fITRUE|FALSE\fB]`True to tell LDMSD
not to start producers automatically .IP
:raw-latex:`\fB[regex\fR=\fIREGEX\fB]`Regular expression to match with
hostnames of peer daemons .IP :raw-latex:`\fB[ip\fR=\fICIDR\fB]`IP Range
in the CIDR format either in IPV4 .RE

:raw-latex:`\fBprdcr`\_listen_start:raw-latex:`\fR `starts accepting
peer advertisement with matches hostnames. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of
prdcr_listen to be started .RE

:raw-latex:`\fBprdcr`\_listen_stop:raw-latex:`\fR `stops accepting peer
advertisement with matches hostnames. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of
prdcr_listen to be stopped .RE

:raw-latex:`\fBprdcr`\_listen_del:raw-latex:`\fR `deletes a
prdcr_listen. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of
prdcr_listen to be deleted .RE

:raw-latex:`\fBprdcr`\_listen_status:raw-latex:`\fR `report the status
of each prdcr_listen object. There is no parameter.

.SH EXAMPLE

In this example, there are three LDMS daemons running on
:raw-latex:`\fBnode-1`:raw-latex:`\fR`,
:raw-latex:`\fBnode-2`:raw-latex:`\fR`, and
:raw-latex:`\fBnode03`:raw-latex:`\fR`. LDMSD running on
:raw-latex:`\fBnode-1`:raw-latex:`\fR `and
:raw-latex:`\fBnode-2`:raw-latex:`\fR` are sampler daemons, namely
:raw-latex:`\fBsamplerd-1`:raw-latex:`\fR `and
:raw-latex:`\fBsamplerd-2`:raw-latex:`\fR`. The aggregator
(:raw-latex:`\fBagg11`:raw-latex:`\fR`) runs on
:raw-latex:`\fBnode-3`:raw-latex:`\fR`. All LDMSD listen on port 411.

The sampler daemons collect the
:raw-latex:`\fBmeminfo`:raw-latex:`\fR `set, and they are configured to
advertise themselves and connect to the aggregator using sock on host
:raw-latex:`\fBnode-3`:raw-latex:`\fR `at port 411. They will try to
reconnect to the aggregator every 10 seconds until the connection is
established. Once the connection is established, they will send an
advertisement to the aggregator. The following are the configuration
files of the :raw-latex:`\fBsamplerd-1`:raw-latex:`\fR `and
:raw-latex:`\fBsamplerd-2`:raw-latex:`\fR`.

.EX .B > cat samplerd-1.conf .RS 4 # Add and start an advertisement
advertiser_add name=agg11 xprt=sock host=node-3 port=411 reconnect=10s
advertiser_start name=agg11 # Load, configure, and start the meminfo
plugin load name=meminfo config name=meminfo producer=samplerd-1
instance=samplerd-1/meminfo start name=meminfo interval=1s .RE

.B > cat samplerd-2.conf .RS 4 # Add and start an advertisement using
only the advertiser_start command advertiser_start name=agg11
host=node-3 port=411 reconnect=10s # Load, configure, and start the
meminfo plugin load name=meminfo config name=meminfo producer=samplerd-2
instance=samplerd-2/meminfo start name=meminfo interval=1s .RE .EE

The aggregator is configured to accept advertisements from the sampler
daemons that the hostnames match the regular expressions
:raw-latex:`\fBnode0`[1-2]:raw-latex:`\fR`.

.EX .B > cat agg.conf .RS 4 # Accept advertisements sent from LDMSD
running on hostnames matched node-[1-2] prdcr_listen_add name=computes
regex=node-[1-2] prdcr_listen_start name=computes # Add and start an
updater updtr_add name=all_sets interval=1s offset=100ms updtr_prdcr_add
name=all_sets regex=.\* updtr_start name=all_sets .RE .EE

LDMSD provides the command
:raw-latex:`\fBadvertiser`\_status:raw-latex:`\fR `to report the status
of advertisement of a sampler daemon.

.EX .B > ldmsd_controller -x sock -p 411 -h node-1 Welcome to the LDMSD
control processor sock:node-1:411> advertiser_status Name Aggregator
Host Aggregator Port Transport Reconnect (us) State —————- —————- —————
———— ————— ———— agg11 node-3 411 sock 10000000 CONNECTED
sock:node-1:411> .EE

Similarly, LDMSD provides the command
:raw-latex:`\fBprdcr`\_listen_status:raw-latex:`\fR `to report the
status of all prdcr_listen objects on an aggregator. The command also
reports the list of auto-added producers corresponding to each
prdcr_listen object.

.EX .B > ldmsd_controller -x sock -p 411 -h node-3 Welcome to the LDMSD
control processor sock:node-3:411> prdcr_listen_status Name State Regex
IP Range ——————– ———- ————— —————————— computes running node-[1-2] -
Producers: node-1:411, node-2:411 sock:node-3:411> .EE

.SH SEE ALSO .BR ldmsd (8) .BR ldmsd_controller (8)
