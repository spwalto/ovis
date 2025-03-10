EXTRA_DIST=

dist_man1_MANS= \
ldms-csv-anonymize.man \
ldms-csv-export-sos.man \
ldms-lsdate.man

dist_man8_MANS= \
ldms-build-install.man \
ldms-ls.man \
ldms-static-test.man \
ldmsd.man \
ldms-plugins.man \
ldmsd-exits.man \
ldmsd-controller.man \
ldmsd-yaml-parser.man \
ldmsctl.man

dist_man7_MANS= \
ldms-authentication.man \
ldms-quickstart.man \
ldms-sampler-base.man \
ldmsd-failover.man \
ldmsd-setgroup.man \
ldms-jobid.man \
ldms-store-csv.man \
ldms-store-sos.man \
ldms-store-papi.man \
ldms-store-app.man \
ldms-store-kafka.man \
ldms-stream.man

if ENABLE_SLURMTEST
dist_man8_MANS += ldms-ldms-pll-ldms-static-test.man
endif
EXTRA_DIST += ldms-ldms-pll-ldms-static-test.man

dist_man7_MANS += ldms-store-rabbitv3.man
dist_man7_MANS += ldms-store-rabbitkw.man

ldmsd-exits.man: $(srcdir)/../src/ldmsd/ldmsd.c $(srcdir)/../src/ldmsd/ldmsd_config.c
	$(srcdir)/make_exits_man.sh $(srcdir)/../src/ldmsd/ldmsd.c $(srcdir)/../src/ldmsd/ldmsd_config.c > ldmsd-exits.man

CLEANFILES=ldmsd-exits.man
