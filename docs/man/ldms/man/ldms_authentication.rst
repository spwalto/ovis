.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for ldms_authentication ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “28 Feb 2018” “v4” “LDMS
Authentication man page”

.SH NAME ldms_authentication - Authentication in LDMS transports

.SH DESCRIPTION LDMS applications use authentication plugins in LDMS
transports to authenticate the peers. In other words, not only
:raw-latex:`\fBldmsd`:raw-latex:`\fR `authenticates the client
connections, the clients .RB ( ldms_ls , .BR ldmsctl , .BR
ldmsd_controller , and other .BR ldmsd ) authenticate the .B ldmsd too.

.BR ldmsd , .BR ldms_ls , .BR ldmsd_controller “, and” .B ldmsctl use
the following options for authentication purpose: .TP .BI -a "
AUTH_PLUGIN" Specifying the name of the authentication plugin. The
default is “none” (no authentication). .TP .BI -A " NAME" = “VALUE”
Specifying options to the authentication plugin. This option can be
given multiple times.

.PP :raw-latex:`\fBauth`:raw-latex:`\fR `configuration object has been
introduced in :raw-latex:`\fBldmsd`:raw-latex:`\fR `version 4.3.4. It
describes an authentication domain in the configuration file with
:raw-latex:`\fBauth`\_add:raw-latex:`\fR `command.
:raw-latex:`\fBlisten`:raw-latex:`\fR `and
:raw-latex:`\fBprdcr`\_add:raw-latex:`\fR `config commands can refer to
:raw-latex:`\fBauth`:raw-latex:`\fR `object created by
:raw-latex:`\fBauth`\_add:raw-latex:`\fR `command to specify the
authentication domain a listening port or a producer connection belong
to. If no :raw-latex:`\fBauth`:raw-latex:`\fR `option is specified,
:raw-latex:`\fBlisten`:raw-latex:`\fR `and
:raw-latex:`\fBprdcr`\_add:raw-latex:`\fR `commands fall back to use the
authentication method specified by :raw-latex:`\fB`-a,
-A:raw-latex:`\fR `CLI options (which is default to
:raw-latex:`\fBnone`:raw-latex:`\fR`).

.PP Please consult the manual of the plugin for more details.

.SH LIST OF LDMS_AUTH PLUGINS

.TP .B none Authentication will NOT be used (allow all connections) .RB
“(see” ldms_auth_none (7)).

.TP .B ovis The shared secret authentication using ovis_ldms .RB “(see”
ldms_auth_ovis (7)).

.TP .B naive The naive authentication for testing. .RB “(see”
ldms_auth_naive (7)).

.TP .B munge User credential authentication using Munge. .RB “(see”
ldms_auth_munge (7)).

.SH SEE ALSO .BR ldms_auth_none (7), .BR ldms_auth_ovis (7), .BR
ldms_auth_naive (7), .BR ldms_auth_munge (7), .BR ldmsctl (8), .BR ldmsd
(8), .BR ldms_ls (8), .BR ldmsd_controller (8), .BR ldms_quickstart (7),
.BR ldms_build_install (7)
