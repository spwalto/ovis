.. role:: raw-latex(raw)
   :format: latex
..

.TH man 7 “11 Sep 2024” “v4” “ldmsd_qgroup man page”

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH NAME ldmsd_qgroup - Quota Group Feature in LDMSD

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH SYNOPSIS

.nh " no hyphenation .ad l "left justified

.IP :raw-latex:`\fBqgroup`\_config:raw-latex:`\fR 14` .RI [quota= BYTES
] .RI [ask_interval= TIME ] .RI [ask_amount= BYTES ] .RI [ask_mark=
BYTES ] .RI [reset_interval= TIME ]

.IP :raw-latex:`\fBqgroup`\_member_add:raw-latex:`\fR 18` .RI xprt= XPRT
.RI host= HOST .RI [port= PORT ] .RI [auth= AUTH ]

.IP :raw-latex:`\fBqgroup`\_member_del:raw-latex:`\fR 18` .RI host= HOST
.RI [port= PORT ]

.IP :raw-latex:`\fBqgroup`\_start:raw-latex:`\fR`

.IP :raw-latex:`\fBqgroup`\_stop:raw-latex:`\fR`

.IP :raw-latex:`\fBqgroup`\_info:raw-latex:`\fR`

.hy 14 " default hyphenation .ad " restore text justification

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH DESCRIPTION

Quota Group (:raw-latex:`\fBqgroup`:raw-latex:`\fR`) is a feature in
LDMS to restrict the flow of LDMS Stream data through the group in a
time interval to prevent the LDMS stream from dominating the network
usage and significantly affecting the running application.
:raw-latex:`\fBqgroup`:raw-latex:`\fR `consists of multiple
:raw-latex:`\fBldmsd`:raw-latex:`\fR `processes that can donate unused
:raw-latex:`\fBquota`:raw-latex:`\fR `to other member processes that
need it, so that we can appropriately utilize the allowed network
bandwidth.

Please see :raw-latex:`\fBQGROUP `MECHANISM:raw-latex:`\fR `section on
how :raw-latex:`\fBqgroup`:raw-latex:`\fR `works and the meaning of the
configuration parameters. The
:raw-latex:`\fBQGROUP `CONFIGURATION:raw-latex:`\fB` section contains
ldmsd commands related to
:raw-latex:`\fBqgroup`:raw-latex:`\fR `manipulation.

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH QGROUP MECHANISM

Each ldmsd participating in :raw-latex:`\fBqgroup`:raw-latex:`\fR `has
to be configured such in add all other members into its
:raw-latex:`\fBqgroup`:raw-latex:`\fR `member list using
:raw-latex:`\fBqgroup`\_member_add:raw-latex:`\fR` commands. This is so
that each member can communicate to each other regarding quota
requesting/donating. In addition, the ldmsd in
:raw-latex:`\fBqgroup`:raw-latex:`\fR `should have per-connection
:raw-latex:`\fBquota`:raw-latex:`\fR `set (in
:raw-latex:`\fBlisten`:raw-latex:`\fR `and
:raw-latex:`\fBprdcr`\_add:raw-latex:`\fR `command) to limit peer’s
outstanding stream data holding in the member processes. If this
per-connection :raw-latex:`\fBquota`:raw-latex:`\fR `is not set
(i.e. being UNLIMTED), the peers can send unlimited amount of data to
the processes in the :raw-latex:`\fBqgroup`:raw-latex:`\fR`.

Let’s setup a simple :raw-latex:`\fBldmsd`:raw-latex:`\fR`’s cluster to
explain the :raw-latex:`\fBqgroup`:raw-latex:`\fR` mechanism. There are
6 daemons: 4 samplers (samp.[1-4]) an 2 L1 aggregators (L1.[1-2]). L1.1
connects (:raw-latex:`\fBprdcr`\_add:raw-latex:`\fR`) samp.1 and samp.2.
L1.2 connects to samp.3 and samp.4. Both L1.1 and L1.2 are in
:raw-latex:`\fBqgroup`:raw-latex:`\fR `(i.e. they
:raw-latex:`\fBqgroup`\_member_add:raw-latex:`\fR `each other). The
:raw-latex:`\fBprdcr`\_add.quota:raw-latex:`\fR `of L1.1 and L1.2 is set
to 6, and they are tracked by the samplers. The square filled boxes (■)
represent available :raw-latex:`\fBquota`:raw-latex:`\fR `for
:raw-latex:`\fBpublish`:raw-latex:`\fR `operation on the samplers. The
filled diamonds (◆) on L1.1 and L1.2 represent available “return”
:raw-latex:`\fBquota`:raw-latex:`\fR`. Normally, when an L1 daemon
finishes processing the stream data, it returns the quota to the
corresponding peer right away. With
:raw-latex:`\fBqgroup`:raw-latex:`\fR`, The L1 damon will take the
return quota from the available return quota (◆) before returning the
quota back to the corresponding peer. If there is not enough available
return quota, the L1 daemon delays the return (in a queue) until there
is enough available return quota.

┌──────┐ │samp.1│ │■■■■■■├──────┐ └──────┘ │ ┌────────────────┐
└─────┤L1.1 ▽ ├───┄ ┌──────┐ ┌─────┤◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆│ │samp.2│ │
└───────┬────────┘ │■■■■■■├──────┘ │ └──────┘ │ │ ┌──────┐ │ │samp.3│ │
│■■■■■■├──────┐ │ └──────┘ │ ┌───────┴────────┐ └─────┤L1.2 ▽ ├───┄
┌──────┐ ┌─────┤◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆│ │samp.4│ │ └────────────────┘
│■■■■■■├──────┘ └──────┘

As things progress, L1’s available return quota (referred to as
:raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `for distinction) will
eventually run low and won’t be able to return the quota back to any
peer anymore. When this happens the peer quota (for publishing)
eventually runs out as seen below.

┌──────┐ │samp.1│ │■■□□□□├──────┐ └──────┘ │ ┌────────────────┐
└─────┤L1.1 ▽ ├───┄ ┌──────┐ ┌─────┤◆◇◇◇◇◇◇◇◇◇◇◇◇◇◇◇│ │samp.2│ │
└───────┬────────┘ │□□□□□□├──────┘ │ └──────┘ │ │ ┌──────┐ │ │samp.3│ │
│■■■■■■├──────┐ │ └──────┘ │ ┌───────┴────────┐ └─────┤L1.2 ▽ ├───┄
┌──────┐ ┌─────┤◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆│ │samp.4│ │ └────────────────┘
│■■■■■■├──────┘ └──────┘

When the :raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `is low,
i.e. :raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `◆ lower than the
threshold :raw-latex:`\fBask`\_mark:raw-latex:`\fR `(denoted as ▽ in the
figure), the daemon asks for a donation from all other members. To
prevent from asking too frequently, the
:raw-latex:`\fBqgroup`:raw-latex:`\fR `members ask other members in
:raw-latex:`\fBask`\_interval:raw-latex:`\fR`. The amount to ask for is
set by :raw-latex:`\fBask`\_amount:raw-latex:`\fR `parameter. The
members who are asked for the donation may not donate fully or may not
donate at all, depending on the members’
:raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `level.

┌──────┐ │samp.1│ │■■□□□□├──────┐ └──────┘ │ ┌────────────────┐
└─────┤L1.1 ▽ ├───┄ ┌──────┐ ┌─────┤◆◆◆◆◆◆◆◆◇◇◇◇◇◇◇◇│ │samp.2│ │
└───────┬────────┘ │□□□□□□├──────┘ │ └──────┘ │ │ ┌──────┐ │ │samp.3│ │
│■■■■■■├──────┐ │ └──────┘ │ ┌───────┴────────┐ └─────┤L1.2 ▽ ├───┄
┌──────┐ ┌─────┤◆◆◆◆◆◆◆◆◆◇◇◇◇◇◇◇│ │samp.4│ │ └────────────────┘
│■■■■■■├──────┘ └──────┘

Asking/donating :raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `allows the
busy members to continue working while reducing the unused
:raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `in the less busy members in
the :raw-latex:`\fBqgroup`:raw-latex:`\fR`. The
:raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `in all members will
eventually run out, and no stream data will be able to go through the
group – restricting LDMS stream network usage.

The :raw-latex:`\fBqgroup`.quota:raw-latex:`\fR `of each member in the
:raw-latex:`\fBqgroup`:raw-latex:`\fR `resets to its original value in
:raw-latex:`\fBreset`\_interval:raw-latex:`\fR `time interval, and the
quota returning process continues.

The maxmum amount of stream data that go through the group per unit time
can be calculated by:

::

           \fBN\fR * \fBqgroup.quota\fR
           ────────────────
            \fBreset_interval\fR

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH QGROUP COMMANDS

.nh " no hyphenation .ad l "left justified .IP
:raw-latex:`\fBqgroup`\_config:raw-latex:`\fR 14` .RI [quota= BYTES ]
.RI [ask_interval= TIME ] .RI [ask_amount= BYTES ] .RI [ask_mark= BYTES
] .RI [reset_interval= TIME ] .hy 14 " default hyphenation .ad " restore
text justification .RS 4 .PP Configure the specified qgroup parameters.
The parameters not specifying to the command will be left untouched. .TP
.BI “[quota=" BYTES ] The amount of our quota (bytes). The
:raw-latex:`\fIBYTES`:raw-latex:`\fR `can be expressed with quantifiers,
e.g. ”1k" for 1024 bytes. The supported quantifiers are “b” (bytes), “k”
(kilobytes), “m” (megabytes), “g” (gigabytes) and “t” (terabytes). .TP
.BI “[ask_interval=" TIME ] The time interval to ask the members when
our quota is low. The :raw-latex:`\fITIME`:raw-latex:`\fR `can be
expressed with units, e.g. ”1s“, but will be treated as microseconds if
no units is specified. The supported units are”us" (microseconds), “ms”
(milliseconds), “s” (seconds), “m” (minutes), “h” (hours), and “d”
(days). .TP .BI “[ask_amount=" BYTES ] The amount of quota to ask from
our members. The :raw-latex:`\fIBYTES`:raw-latex:`\fR `can be expressed
with quantifiers, e.g. ”1k" for 1024 bytes. The supported quantifiers
are “b” (bytes), “k” (kilobytes), “m” (megabytes), “g” (gigabytes) and
“t” (terabytes). .TP .BI “[ask_mark=" BYTES ] The amount of quota to
determine as ‘low’, to start asking quota from other members. The
:raw-latex:`\fIBYTES`:raw-latex:`\fR `can be expressed with quantifiers,
e.g. ”1k" for 1024 bytes. The supported quantifiers are “b” (bytes), “k”
(kilobytes), “m” (megabytes), “g” (gigabytes) and “t” (terabytes). .TP
.BI “[reset_interval=" TIME ] The time interval to reset our quota to
its original value. The :raw-latex:`\fITIME`:raw-latex:`\fR `can be
expressed with units, e.g. ”1s“, but will be treated as microseconds if
no units is specified. The supported units are”us" (microseconds), “ms”
(milliseconds), “s” (seconds), “m” (minutes), “h” (hours), and “d”
(days). .RE

.nh " no hyphenation .ad l "left justified .IP
:raw-latex:`\fBqgroup`\_member_add:raw-latex:`\fR 18` .RI xprt= XPRT .RI
host= HOST .RI [port= PORT ] .RI [auth= AUTH ] .hy 14 " default
hyphenation .ad " restore text justification .RS 4 .PP Add a member into
the process’ qgroup member list. .TP .BI “xprt=” XPRT The transport type
of the connection (e.g. “sock”). .TP .BI “host=” HOST The hostname or IP
address of the member. .TP .BI “[port=" PORT ] The port of the member
(default: 411). .TP .BI”[auth=" AUTH_REF ] The reference to the
authentication domain (the :raw-latex:`\fBname`:raw-latex:`\fR `in
:raw-latex:`\fBauth`\_add:raw-latex:`\fR` command) to be used in this
connection If not specified, the default authentication domain of the
daemon is used. .RE

.nh " no hyphenation .ad l "left justified .IP
:raw-latex:`\fBqgroup`\_member_del:raw-latex:`\fR 18` .RI host= HOST .RI
[port= PORT ] .hy 14 " default hyphenation .ad " restore text
justification .RS 4 .PP Delete a member from the list. .TP .BI “host”
HOST The hostname or IP address of the member. .TP .BI "[port " PORT ]
The port of the member (default: 411). .RE

.nh " no hyphenation .ad l "left justified .IP
:raw-latex:`\fBqgroup`\_start:raw-latex:`\fR` .hy 14 " default
hyphenation .ad " restore text justification .RS 4 .PP Start the qgroup
service. .RE

.nh " no hyphenation .ad l "left justified .IP
:raw-latex:`\fBqgroup`\_stop:raw-latex:`\fR` .hy 14 " default
hyphenation .ad " restore text justification .RS 4 .PP Stop the qgroup
service. .RE

.nh " no hyphenation .ad l "left justified .IP
:raw-latex:`\fBqgroup`\_info:raw-latex:`\fR` .hy 14 " default
hyphenation .ad " restore text justification .RS 4 .PP Print the qgroup
information (e.g. current quota value, parameter values, member
connection states, etc). .RE

.""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""/.
.SH EXAMPLE .nh " no hyphenation .ad l "left justified

.IP qgroup_config 14 quota=1M ask_interval=200ms ask_mark=200K
ask_amount=200K reset_interval=1s

.IP qgroup_member_add 18 host=node-2 port=411 xprt=sock auth=munge

.IP qgroup_member_add 18 host=node-3 port=411 xprt=sock auth=munge

.IP qgroup_start

.hy 14 " default hyphenation .ad " restore text justification

."""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""“/.
.SH SEE ALSO .BR ldmsd”(8), " ldmsd_controller “(8),” ldms_quickstart
“(7)”
