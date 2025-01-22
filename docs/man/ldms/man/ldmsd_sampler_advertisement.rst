.. role:: raw-latex(raw)
   :format: latex
..

" Manpage for ldmsd_sampler_advertisement .TH man 7 “27 March 2024” “v5”
“LDMSD Sampler Advertisement man page”

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH NAME ldmsd_sampler_advertisement - Manual for LDMSD Sampler
Advertisement

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH SYNOPSIS

**Sampler side Commands**

.IP :raw-latex:`\fBadvertiser`\_add .RI “name=” NAME " xprt=" XPRT "
host=" HOST " port=" PORT .RI “[auth=" AUTH_DOMAIN "]”

.IP :raw-latex:`\fBadvertiser`\_start .RI “name=” NAME .RI “[xprt=" XPRT
" host=" HOST " port=" PORT " auth=" AUTH_DOMAIN "]”

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

LDMSD Sampler Discovery is a capability that enables LDMSD to
automatically add producers whose hostnames match a given regular
expression or whose IP addresses fall in a given IP range. If neither
regular expression nor an IP range is given, LDMSD adds a producer
whenever it receives an advertisement message. The feature eliminates
the need for manual configuration of sampler hostnames in the aggregator
configuration file.

Admins specify the aggregator hostname and the listening port in sampler
configuration via the
:raw-latex:`\fBadvertiser`\_add:raw-latex:`\fR `command and start the
advertisement with the
:raw-latex:`\fBadvertiser`\_start:raw-latex:`\fR `command. The sampler
now advertises its hostname to the aggregator. On the aggregator, admins
may specify a regular expression to be matched with the sampler hostname
or an IP range via the
:raw-latex:`\fBprdcr`\_listen_add:raw-latex:`\fR `command. The
:raw-latex:`\fBprdcr`\_listen_start:raw-latex:`\fR `command is used to
tell the aggregator to automatically add producers corresponding to a
sampler of which the hostname matches the regular expression or the IP
address falls in the given IP range.

The automatically added producers are of the ‘advertised’ type. The
producer’s name is the same as the value of the ‘name’ attribute given
at the :raw-latex:`\fBadvertiser`\_add:raw-latex:`\fR `line in the
sampler configuration file. LDMSD automatically starts the advertised
producers. Admins could provide the
:raw-latex:`\fBdisable`\_start:raw-latex:`\fR `attribute at the
:raw-latex:`\fBprdcr`\_listen_add:raw-latex:`\fR `with the ‘true’ value
to let LDMSD not automatically start the advertised producers. Admins
can stop an advertised producer using the
:raw-latex:`\fBprdcr`\_stop:raw-latex:`\fR `or
:raw-latex:`\fBprdcr`\_stop_regex:raw-latex:`\fR `commands. They can be
restarted by using the :raw-latex:`\fBprdcr`\_start:raw-latex:`\fR `or
:raw-latex:`\fBprdcr`\_start_regex:raw-latex:`\fR `commands.

The description for each command and its parameters are as follows.

**Sampler Side Commands**

:raw-latex:`\fBadvertiser`\_add:raw-latex:`\fR `adds a new
advertisement. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` String of the
advertisement name. The aggregator uses the string as the producer name
as well. .IP :raw-latex:`\fBhost`:raw-latex:`\fR`=:raw-latex:`\fIHOST`
Aggregator hostname .IP
:raw-latex:`\fBxprt`:raw-latex:`\fR`=:raw-latex:`\fIXPRT` Transport to
connect to the aggregator .IP
:raw-latex:`\fBport`:raw-latex:`\fR`=:raw-latex:`\fIPORT` Listen port of
the aggregator .IP
:raw-latex:`\fBreconnect`:raw-latex:`\fR`=:raw-latex:`\fIINTERVAL`
Reconnect interval d .IP :raw-latex:`\fB[auth\fR=\fIAUTH_DOMAIN\fB]`The
authentication domain to be used to connect to the aggregator .RE

:raw-latex:`\fBadvertiser`\_start:raw-latex:`\fR `starts an
advertisement. If the advertiser does not exist, LDMSD will create the
advertiser. In this case, the mandatory attributes for
:raw-latex:`\fBadvertiser`\_add:raw-latex:`\fB `must be given. The
parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` The
advertisement name to be started .IP
:raw-latex:`\fB[host\fR=\fIHOST\fB]`Aggregator hostname .IP
:raw-latex:`\fB[xprt\fR=\fIXPRT\fB]`Transport to connect to the
aggregator .IP :raw-latex:`\fB[port\fR=\fIPORT\fB]`Listen port of the
aggregator .IP :raw-latex:`\fB[reconnect\fR=\fIINTERVAL\fB]`Reconnect
interval .IP :raw-latex:`\fB[auth\fR=\fIAUTH_DOMAIN\fB]`The
authentication domain to be used to connect to the aggregator .RE

:raw-latex:`\fBadvertiser`\_stop:raw-latex:`\fR `stops an advertisement.
The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` The
advertisement name to be stopped .RE

:raw-latex:`\fBadvertiser`\_del:raw-latex:`\fR `deletes an
advertisement. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` The
advertisement name to be deleted .RE

:raw-latex:`\fBadvertiser`\_status reports the status of each
advertisement. An optional parameter is: .RS .IP
:raw-latex:`\fB[name\fR=\fINAME\fB]`Advertisement name .RE

.PP **Aggregator Side commands**

:raw-latex:`\fBprdcr`\_listen_add:raw-latex:`\fR `adds a regular
expression to match sampler advertisements. The parameters are: .RS .IP
:raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` String of the
prdcr_listen name. .IP
:raw-latex:`\fB[disable_start\fR=\fITRUE|FALSE\fB]`True to tell LDMSD
not to start producers automatically .IP
:raw-latex:`\fB[regex\fR=\fIREGEX\fB]`Regular expression to match with
hostnames in sampler advertisements .IP
:raw-latex:`\fBip`:raw-latex:`\fR`=:raw-latex:`\fICIDR`:raw-latex:`\fB`]
IP Range in the CIDR format either in IPV4 or IPV6 .RE

:raw-latex:`\fBprdcr`\_listen_start:raw-latex:`\fR `starts accepting
sampler advertisement with matches hostnames. The parameters are: .RS
.IP :raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of
prdcr_listen to be started .RE

:raw-latex:`\fBprdcr`\_listen_stop:raw-latex:`\fR `stops accepting
sampler advertisement with matches hostnames. The parameters are: .RS
.IP :raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME` Name of
prdcr_listen to be stopped .RE

:raw-latex:`\fBprdcr`\_listen_del:raw-latex:`\fR `deletes a regular
expression to match hostnames in sampler advertisements. The parameters
are: .RS .IP :raw-latex:`\fBname`:raw-latex:`\fR`=:raw-latex:`\fINAME`
Name of prdcr_listen to be deleted .RE

:raw-latex:`\fBprdcr`\_listen_status:raw-latex:`\fR `report the status
of each prdcr_listen object. There is no parameter.

.SH Managing Receive Quota and Rate Limits for Auto-Added Producers

The receive quota and rate limit control machanisms govern the amount of
data a producer receives from the data source connected through
ldms_xprt. This helps prevent data bursts that could overwhelm the LDMS
daemon host and network resources. To configure receive quota and rate
limits, users can create a listening endpoint on the aggregator using
the :raw-latex:`\fBlisten`:raw-latex:`\fR `command specifying the
desired values of the :raw-latex:`\fBquota`:raw-latex:`\fR `and
:raw-latex:`\fBrx`\_rate:raw-latex:`\fR `attributes. Moreover, users
configure the sampler daemons to advertise to the listening endpoint
created on the aggregator, including the preferred receive quota and
rate limit values.

.SH EXAMPLE

In this example, there are three LDMS daemons running on
:raw-latex:`\fBnode-1`:raw-latex:`\fR`,
:raw-latex:`\fBnode-2`:raw-latex:`\fR`, and
:raw-latex:`\fBnode03`:raw-latex:`\fR`. LDMSD running on
:raw-latex:`\fBnode-1`:raw-latex:`\fR `and
:raw-latex:`\fBnode-2`:raw-latex:`\fR` are sampler daemons, namely
:raw-latex:`\fBsamplerd-1`:raw-latex:`\fR `and
:raw-latex:`\fBsamplerd-2`:raw-latex:`\fR`. The aggregator
(:raw-latex:`\fBagg`:raw-latex:`\fR`) runs on
:raw-latex:`\fBnode-3`:raw-latex:`\fR`. All LDMSD listen on port 411.

The sampler daemons collect the
:raw-latex:`\fBmeminfo`:raw-latex:`\fR `set, and they are configured to
advertise themselves and connect to the aggregator using sock on host
:raw-latex:`\fBnode-3`:raw-latex:`\fR `at port 411. They will try to
reconnect to the aggregator every 10 seconds until the connection is
established. The following are the configuration files of the
:raw-latex:`\fBsamplerd-1`:raw-latex:`\fR `and
:raw-latex:`\fBsamplerd-2`:raw-latex:`\fR`.

.EX .B > cat samplerd-1.conf .RS 4 # Create a listening endpoint listen
xprt=sock port=411 # Add and start an advertisement advertiser_add
name=samplerd-1 xprt=sock host=node-3 port=411 reconnect=10s
advertiser_start name=samplerd-1 # Load, configure, and start the
meminfo plugin load name=meminfo config name=meminfo producer=samplerd-1
instance=samplerd-1/meminfo start name=meminfo interval=1s .RE

.B > cat samplerd-2.conf .RS 4 # Create a listening endpoint listen
xprt=sock port=411 # Add and start an advertisement using only the
advertiser_start command advertiser_start name=samplerd-2 host=node-3
port=411 reconnect=10s # Load, configure, and start the meminfo plugin
load name=meminfo config name=meminfo producer=samplerd-2
instance=samplerd-2/meminfo start name=meminfo interval=1s .RE .EE

The aggregator is configured to accept advertisements from the sampler
daemons that the hostnames match the regular expressions
:raw-latex:`\fBnode0`[1-2]:raw-latex:`\fR`. The name of the auto-added
producers is the name of the advertiser on the sampler daemons.

.EX .B > cat agg.conf .RS 4 # Create a listening endpoint listen
xprt=sock port=411 # Accept advertisements sent from LDMSD running on
hostnames matched node-[1-2] prdcr_listen_add name=computes
regex=node-[1-2] prdcr_listen_start name=computes # Add and start an
updater updtr_add name=all_sets interval=1s offset=100ms updtr_prdcr_add
name=all_sets regex=.\* updtr_start name=all .RE .EE

LDMSD provides the command
:raw-latex:`\fBadvertiser`\_status:raw-latex:`\fR `to report the status
of advertisement of a sampler daemon.

.EX .B > ldmsd_controller -x sock -p 10001 -h node-1 Welcome to the
LDMSD control processor sock:node-1:10001> advertiser_status Name
Aggregator Host Aggregator Port Transport Reconnect (us) State —————-
—————- ————— ———— ————— ———— samplerd-1 node-3 411 sock 10000000
CONNECTED sock:node-1:10001> .EE

Similarly, LDMSD provides the command
:raw-latex:`\fBprdcr`\_listen_status:raw-latex:`\fR `to report the
status of all prdcr_listen objects on an aggregator. The command also
reports the list of auto-added producers corresponding to each
prdcr_listen object.

.EX .B > ldmsd_controller -x sock -p 10001 -h node-3 Welcome to the
LDMSD control processor sock:node-3:10001> prdcr_listen_status Name
State Regex IP Range ——————– ———- ————— —————————— computes running
node-[1-2] - Producers: samplerd-1, samplerd-2 sock:node-3:10001> .EE

Next is an example that controls the receive quota and rate limits of
the auto-added producers on agg11. Similar to the first example, the
aggregator, agg11, listens on port 411 and waits for advertisements.
Moreover, a listening endpoint on port 412 is added with a receive quota
value. The aggregator also creates producers when an advertisement sent
from the host its IP address falling into the subnet 192.168.0.0:16.

.EX .B > cat agg11.conf .RS 4 # Create a listening endpoint listen
xprt=sock port=411 # Create the listening endpoint for receiving
advertisement listen xprt=sock port=412 quota=4000 # Accept
advertisements sent from LDMSD running on hostnames their IP address #
falling in the range 192.168.0.0:16. prdcr_listen_add name=compute
ip=192.168.0.0:16 prdcr_listen_start name=compute # Add and start an
updater updtr_add name=all_sets interval=1s offset=100ms updtr_prdcr_add
name=all_sets regex=.\* updtr_start name=all .RE .EE

There are two sampler daemons, which are configured to advertise to port
412 so that the auto-added producers adopt the receive credidts of the
listening endpoint on port 412.

.EX .B > cat samplerd-3.conf .RS 4 # Create a listening endpoint listen
xprt=sock port=411 # Start an advertiser that sends the advertisement to
port 412 on the aggregator # host advertiser_start name=samplerd-3
host=agg11 xprt=sock port=412 reconnect=10s # Load, configure, and start
the meminfo plugin load name=meminfo config name=meminfo
producer=samplerd-3 instance=samplerd-3/meminfo start name=meminfo
interval=1s .RE .EE

.EX .B > cat samplerd-4.conf .RS 4 # Create a listening endpoint listen
xprt=sock port=411 # Start an advertiser that sends the advertisement to
port 412 on the aggregator # host advertiser_start name=samplerd-4
host=agg11 xprt=sock port=412 reconnect=10s # Load, configure, and start
the meminfo plugin load name=meminfo config name=meminfo
producer=samplerd-4 instance=samplerd-4/meminfo start name=meminfo
interval=1s .RE .EE

.SH SEE ALSO .BR ldmsd (8) .BR ldmsd_controller (8)
