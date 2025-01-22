.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for ldms_auth_munge ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “10 May 2018” “v4” “ldms_auth_munge”

.SH NAME ldms_auth_munge - LDMS authentication using munge

.SH SYNOPSIS .HP .I ldms_app .BI “-a munge [-A socket=PATH ]”

.SH DESCRIPTION :raw-latex:`\fBldms`\_auth_munge:raw-latex:`\fR `relies
on the :raw-latex:`\fBmunge`:raw-latex:`\fR `service (see
:raw-latex:`\fBmunge`:raw-latex:`\fR`(7)) to authenticate users. The
munge daemon (:raw-latex:`\fBmunged`:raw-latex:`\fR`) must be up and
running.

The optional :raw-latex:`\fBsocket`:raw-latex:`\fR `option can be used
to specify the path to the munged unix domain socket in the case that
munged wasn’t using the default path or there are multiple munge domains
configured.

.SH SEE ALSO :raw-latex:`\fBmunge`:raw-latex:`\fR`(7),
:raw-latex:`\fBmunged`:raw-latex:`\fR`(8)
