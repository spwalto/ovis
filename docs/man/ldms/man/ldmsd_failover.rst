.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for ldmsd_failover ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “13 Aug 2018” “v4.1” “LDMSD Failover
man page”

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH NAME ldmsd_failover - explanation, configuration, and commands for
ldmsd failover

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH SYNOPSIS

.nh " no hyphenation .ad l "left justified

.IP :raw-latex:`\fBfailover`\_config:raw-latex:`\fR 16` .RI “host=” HOST
" port=" PORT " xprt=" XPRT .RI “[peer_name=" NAME "] [interval=" USEC
"] [timeout_factor=" FLOAT "]” .RI “[auto_switch=" 0|1 "]”

.IP :raw-latex:`\fBfailover`\_start:raw-latex:`\fR`

.IP :raw-latex:`\fBfailover`\_stop:raw-latex:`\fR`

.IP :raw-latex:`\fBfailover`\_status:raw-latex:`\fR`

.IP :raw-latex:`\fBfailover`\_peercfg_start:raw-latex:`\fR`

.IP :raw-latex:`\fBfailover`\_peercfg_stop:raw-latex:`\fR`

.hy 14 " default hyphenation .ad " restore text justification

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH DESCRIPTION

:raw-latex:`\fBldmsd`:raw-latex:`\fR `can be configured to form a
failover pair with another :raw-latex:`\fBldmsd`:raw-latex:`\fR`. In a
nutshell, when a failover pair is formed, the ldmsd’s exchange their
updater and producer configuration so that when one goes down, the other
will take over the LDMS set aggregation load
(:raw-latex:`\fBfailover`:raw-latex:`\fR`).

:raw-latex:`\fBPing`-echo:raw-latex:`\fR `mechanism is used to detect
the service unavailability. Each ldmsd in the pair sends ping requests
to the other, the peer echo back along with its status. When the echo
has not been received within the timeout period (see below), the peer
configuration is automatically started (failover).

The following paragraphs explain ldmsd configuration commands relating
to ldmsd failover feature.

:raw-latex:`\fBfailover`\_config:raw-latex:`\fR `configure failover
feature in an ldmsd. The failover service must be stopped before
configuring it. The following list describes the command parameters. .RS
.IP
:raw-latex:`\fBhost`:raw-latex:`\fR`=:raw-latex:`\fIHOST`:raw-latex:`\fR 16`
The hostname of the failover partner. This is optional in
re-configuration. .IP
:raw-latex:`\fBport`:raw-latex:`\fR`=:raw-latex:`\fIPORT`:raw-latex:`\fR`
The LDMS port of the failover partner. This is optional in
re-configuration. .IP
:raw-latex:`\fBxprt`:raw-latex:`\fR`=:raw-latex:`\fIXPRT`:raw-latex:`\fR`
The LDMS transport type (sock, rdma, or ugni) of the failover partner.
This is optional in re-configuration. .IP
:raw-latex:`\fBpeer`\_name:raw-latex:`\fR`=:raw-latex:`\fINAME`:raw-latex:`\fR`
(Optional) The ldmsd name of the failover parter (please see option
:raw-latex:`\fB`-n:raw-latex:`\fR `in
:raw-latex:`\fBldmsd`:raw-latex:`\fR`(8)). If this is specified, the
ldmsd will only accept a pairing with other ldmsd with matching name.
Otherwise, the ldmsd will pair with any ldmsd requesting a failover
pairing. .IP
:raw-latex:`\fBinterval`:raw-latex:`\fR`=:raw-latex:`\fIuSEC`:raw-latex:`\fR`
(Optional) The interval (in micro-seconds) for ping and transport
re-connecting. The default is 1000000 (1 sec). .IP
:raw-latex:`\fBtimeout`\_factor:raw-latex:`\fR`=:raw-latex:`\fIFLOAT`:raw-latex:`\fR`
(Optional) The echo timeout factor. The echo timeout is calculated by
:raw-latex:`\fB`%timeout_factor \* %interval:raw-latex:`\fR`. The
default is 2. .IP
:raw-latex:`\fBauto`\_switch:raw-latex:`\fR`=:raw-latex:`\fI0`\|1:raw-latex:`\fR`
(Optional) If this is on (1), ldmsd will start
:raw-latex:`\fBpeercfg`:raw-latex:`\fR `or stop
:raw-latex:`\fBpeercfg`:raw-latex:`\fR `automatically. Otherwise, the
user need to issue
:raw-latex:`\fBfailover`\_peercfg_start:raw-latex:`\fR `or
:raw-latex:`\fBfailover`\_peercfg_stop:raw-latex:`\fR `manually. By
default, this value is 1. .RE

:raw-latex:`\fBfailover`\_start:raw-latex:`\fR `is a command to start
the (configured) failover service. After the failover service has
started, it will pair with the peer, retreiving peer configurations and
start peer configurations when it believes that the peer is not in
service (with ``auto_switch=1``, otherwise it does nothing).

Please also note that when the failover service is in use (after
:raw-latex:`\fBfailover`\_start:raw-latex:`\fR`), prdcr, updtr, and
strgp cannot be altered over the in-band configuration (start, stop, or
reconfigure). The failover service must be stopped
(:raw-latex:`\fBfailover`\_stop:raw-latex:`\fR`) before altering those
configuration objects.

:raw-latex:`\fBfailover`\_stop:raw-latex:`\fR `is a command to stop the
failover service. When the service is stopped, the peer configurations
will also be stopped and removed from the local memory. The peer also
won’t be able to pair with local ldmsd when the failover service is
stopped. Issuing :raw-latex:`\fBfailover`\_stop:raw-latex:`\fR `after
the pairing process succeeded will stop failover service on both daemons
in the pair.

:raw-latex:`\fBfailover`\_status:raw-latex:`\fR `is a command to report
(via :raw-latex:`\fBldmsd`\_controller:raw-latex:`\fR`) the failover
statuses.

:raw-latex:`\fBfailover`\_peercfg_start:raw-latex:`\fR `is a command to
manually start peer configruation. Please note that if the
:raw-latex:`\fBauto`\_switch:raw-latex:`\fR `is 1, the ldmsd will
automatically stop peer configuration when it receives the echo from the
peer.

:raw-latex:`\fBfailover`\_peercfg_stop:raw-latex:`\fR `is a command to
manually stop peer configuration. Please note that if the
:raw-latex:`\fBauto`\_switch:raw-latex:`\fR `is 1, the ldmsd will
automatically start peercfg when the echo has timed out.

.SH FAILOVER: AUTOMATIC PEERCFG ACTIVATION

The peer configuration is automatically activated when an echo-timeout
event occurred (with ``auto_switch=1``). The echo-timeout is calculated
based on ping interval, ping-echo round-trip time, ``timeout_factor``
and moving standard deviation of ping-echo round-trip time as follows:

::

   rt_time[N] is an array of last N ping-echo round-trip time.

   base = max( max(rt_time), ping_interval )
   timeout1 = base + 4 * SD(rt_time)
   timeout2 = base*timeout_factor

   timeout = max( timeout1, timeout2 )

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH EXAMPLES

Let’s consider the following setup:

.EX .——-. \| a20 \| \|——-\| \| s00/a \| \| s00/b \| \| s01/a \| \| s01/b
\| \| s02/a \| \| s02/b \| \| s03/a \| \| s03/b \| ‘——-’ ^ \|
.———–‘———–. \| \| .——-. .——-. \| a10 \| \| a11 \| \|——-\| \|——-\| \|
s00/a \| pair \| s02/a \| \| s00/b \|……………\| s02/b \| \| s01/a \| \|
s03/a \| \| s01/b \| \| s03/b \|’——-’ ‘——-’ ^ ^ \| \| .—-‘—. .-’——. \|
\| \| \| .——-..——-. .——-..——-. \| s00 \|\| s01 \| \| s02 \|\| s03 \|
\|——-||——-\| \|——-||——-\| \| s00/a \|\| s01/a \| \| s02/a \|\| s03/a \|
\| s00/b \|\| s01/b \| \| s02/b \|\| s03/b \| ‘——-’‘——-’ ‘——-’‘——-’ .EE

In this setup, we have 4 sampler daemons
(:raw-latex:`\fIs00`:raw-latex:`\fR `-
:raw-latex:`\fIs03`:raw-latex:`\fR`), 2 level-1 aggregator
(:raw-latex:`\fIa10`:raw-latex:`\fR`,
:raw-latex:`\fIa11`:raw-latex:`\fR`), and 1 level-2 aggregator
(:raw-latex:`\fIa20`:raw-latex:`\fR`). Each sampler daemon contain set
:raw-latex:`\fIa`:raw-latex:`\fR `and set
:raw-latex:`\fIb`:raw-latex:`\fR`, which are prefixed by the sampler
daemon name. The level-1 aggregators are configured to be a failover
pair, aggregating sets from the sampler daemons as shown in the picture.
And the level-2 aggregator is configured to aggregate sets from the
level-1 aggregators.

The following is a list of configuration and CLI options to achieve the
setup shown above:

.EX .B # a20.cfg prdcr_add name=prdcr_a10 host=a10.hostname port=12345
xprt=sock \\ type=active interval=1000000 prdcr_start name=prdcr_a10
prdcr_add name=prdcr_a11 host=a11.hostname port=12345 xprt=sock \\
type=active interval=1000000 prdcr_start name=prdcr_a11 updtr_add
name=upd interval=1000000 offset=0 updtr_prdcr_add name=upd regex.\*
updtr_start upd

.B # a10.cfg prdcr_add name=prdcr_s00 host=s00.hostname port=12345
xprt=sock \\ type=active interval=1000000 prdcr_start name=prdcr_s00
prdcr_add name=prdcr_s01 host=s01.hostname port=12345 xprt=sock \\
type=active interval=1000000 prdcr_start name=prdcr_s01 updtr_add
name=upd interval=1000000 offset=0 updtr_prdcr_add name=upd regex.\*
updtr_start upd
:raw-latex:`\fIfailover`\_config:raw-latex:`\fR `host=a11.hostname
port=12345 xprt=sock \\ interval=1000000 peer_name=a11
:raw-latex:`\fIfailover`\_start:raw-latex:`\fR` .B # a10 CLI $ ldmsd -c
a10.cfg -x sock:12345
:raw-latex:`\fB`-n:raw-latex:`\fR `:raw-latex:`\fIa10`:raw-latex:`\fR` #
name this daemon “a10”

.B # a11.cfg prdcr_add name=prdcr_s02 host=s02.hostname port=12345
xprt=sock \\ type=active interval=1000000 prdcr_start name=prdcr_s02
prdcr_add name=prdcr_s03 host=s03 port=12345 xprt=sock \\ type=active
interval=1000000 prdcr_start name=prdcr_s03 updtr_add name=upd
interval=1000000 offset=0 updtr_prdcr_add name=upd regex.\* updtr_start
upd :raw-latex:`\fIfailover`\_config:raw-latex:`\fR `host=a10.hostname
port=12345 xprt=sock \\ interval=1000000 peer_name=a10
:raw-latex:`\fIfailover`\_start:raw-latex:`\fR` .B # a11 CLI $ ldmsd -c
a11 -x sock:12345
:raw-latex:`\fB`-n:raw-latex:`\fR `:raw-latex:`\fIa11`:raw-latex:`\fR` #
name this daemon “a11”

:raw-latex:`\fB`# sampler config:raw-latex:`\fR `are omitted
(irrelevant). .EE

With this setup, when :raw-latex:`\fIa10`:raw-latex:`\fR `died,
:raw-latex:`\fIa11`:raw-latex:`\fR `will start aggregating sets from
:raw-latex:`\fIs00`:raw-latex:`\fR `and
:raw-latex:`\fIs01`:raw-latex:`\fR`. When this is done,
:raw-latex:`\fIa20`:raw-latex:`\fR `will still get all of the sets
through :raw-latex:`\fIa11`:raw-latex:`\fR `depicted in the following
figure.

.EX .——-. \| a20 \| \|——-\| \| s00/a \| \| s00/b \| \| s01/a \| \| s01/b
\| \| s02/a \| \| s02/b \| \| s03/a \| \| s03/b \| ‘——-’ ^ \| ‘———–. \|
xxxxxxxxx .——-. x a10 x \| a11 \| x——-x \|——-\| x s00/a x \| s00/a \| x
s00/b x \| s00/b \| x s01/a x \| s01/a \| x s01/b x \| s01/b \|
xxxxxxxxx \| s02/a \| \| s02/b \| \| s03/a \| \| s03/b \|’——-’ ^ \|
.——–.—————–.-‘——. \| \| \| \| .——-..——-. .——-..——-. \| s00 \|\| s01 \|
\| s02 \|\| s03 \| \|——-||——-\| \|——-||——-\| \| s00/a \|\| s01/a \| \|
s02/a \|\| s03/a \| \| s00/b \|\| s01/b \| \| s02/b \|\| s03/b
\|’——-’‘——-’ ‘——-’‘——-’ .EE

When :raw-latex:`\fIa10`:raw-latex:`\fR `heartbeat is back,
:raw-latex:`\fIa11`:raw-latex:`\fR `will stop its producers/updaters
that were working in place of :raw-latex:`\fIa10`:raw-latex:`\fR`. The
LDMS network is then recovered back to the original state in the first
figure.

."""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""“/.
.SH SEE ALSO .BR ldmsd”(8), " ldms_quickstart “(7),” ldmsd_controller
“(8)”
